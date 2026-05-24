#include "server/server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "domain/command_dispatcher.h"
#include "domain/command_result.h"
#include "protocol/resp_parser.h"

namespace {

constexpr int k_poll_timeout_ms = 1000;

}  // namespace

void Server::die(const char* msg) {
    const int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

void Server::msg_errno(const char* msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
}

void Server::fd_set_nb(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        die("fcntl(F_GETFL)");
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        die("fcntl(F_SETFL)");
    }
}

bool Server::push_bytes(RingBuffer& buf, const uint8_t* data, size_t len) {
    return buf.push(data, len) == len;
}

Server& Server::getInstance(size_t max_cache_entries, uint16_t port) {
    static Server instance(max_cache_entries, port);
    return instance;
}

Server::Server(size_t max_cache_entries, uint16_t port)
    : ctx_(max_cache_entries), port_(port), listen_fd_(-1) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        die("socket()");
    }

    const int val = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        die("bind");
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        die("listen");
    }

    fd_set_nb(listen_fd_);
}

Server::~Server() {
    cleanup_connections();
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

void Server::update_io_interest(Conn* conn) {
    conn->want_write = !conn->outgoing.empty();
    conn->want_read = !conn->want_close && conn->blocked == BlockReason::None &&
                      conn->incoming.free_space() > 0;
}

bool Server::try_one_request(Conn* conn) {
    if (conn->incoming.used() < 4) {
        return false;
    }

    uint32_t len_be = 0;
    conn->incoming.peek(0, reinterpret_cast<uint8_t*>(&len_be), sizeof(len_be));
    const uint32_t len = ntohl(len_be);
    if (len > k_max_msg) {
        msg_errno("client sent too long message");
        conn->want_close = true;
        return false;
    }
    if (conn->incoming.used() < 4 + len) {
        return false;
    }

    std::vector<uint8_t> payload(len);
    conn->incoming.peek(4, payload.data(), len);

    std::vector<std::string> cmd;
    if (!parse_request(payload.data(), len, cmd)) {
        msg_errno("invalid request");
        conn->want_close = true;
        return false;
    }

    const size_t body_size = estimate_reply_body_size(cmd, ctx_.cache());
    const size_t reply_size = 4 + body_size;

    if (reply_size > conn->outgoing.capacity()) {
        msg_errno("reply too large for outgoing buffer");
        conn->want_close = true;
        return false;
    }
    if (conn->outgoing.free_space() < reply_size) {
        conn->blocked = BlockReason::NeedOutgoingSpace;
        update_io_interest(conn);
        return false;
    }

    const CommandResult result = dispatch_command(cmd, ctx_.cache());
    const std::vector<uint8_t> body = encode_command_result(result);

    const uint32_t body_len_be = htonl(static_cast<uint32_t>(body.size()));
    const auto* body_len_bytes = reinterpret_cast<const uint8_t*>(&body_len_be);
    assert(push_bytes(conn->outgoing, body_len_bytes, sizeof(body_len_be)));
    if (!body.empty()) {
        assert(push_bytes(conn->outgoing, body.data(), body.size()));
    }

    conn->incoming.consume(4 + len);
    conn->blocked = BlockReason::None;
    update_io_interest(conn);
    return true;
}

void Server::process_incoming_requests(Conn* conn) {
    for (size_t i = 0; i < k_max_requests_per_wake && try_one_request(conn); ++i) {
    }
}

void Server::handle_write(Conn* conn) {
    if (conn->outgoing.empty()) {
        return;
    }

    const uint8_t* data = conn->outgoing.read_ptr();
    const size_t n = conn->outgoing.readable_contiguous();
    const ssize_t rv = write(conn->fd, data, n);
    if (rv < 0 && errno == EAGAIN) {
        return;
    }
    if (rv < 0) {
        msg_errno("write() error");
        conn->want_close = true;
        return;
    }
    conn->outgoing.consume(static_cast<size_t>(rv));
}

void Server::drain_outgoing(Conn* conn) {
    while (!conn->outgoing.empty() && !conn->want_close) {
        const size_t before = conn->outgoing.used();
        handle_write(conn);
        if (conn->want_close) {
            return;
        }
        if (conn->outgoing.used() >= before) {
            break;
        }
    }

    conn->blocked = BlockReason::None;
    process_incoming_requests(conn);
    update_io_interest(conn);
}

void Server::handle_read(Conn* conn) {
    if (conn->incoming.free_space() == 0) {
        update_io_interest(conn);
        return;
    }

    uint8_t buf[64 * 1024];
    const size_t to_read = std::min(sizeof(buf), conn->incoming.free_space());
    const ssize_t rv = read(conn->fd, buf, to_read);

    if (rv < 0 && errno == EAGAIN) {
        return;
    }
    if (rv < 0) {
        msg_errno("read() error");
        conn->want_close = true;
        update_io_interest(conn);
        return;
    }
    if (rv == 0) {
        if (conn->incoming.empty()) {
            msg_errno("client disconnected before sending anything");
        } else {
            msg_errno("unexpected EOF");
        }
        conn->want_close = true;
        update_io_interest(conn);
        return;
    }

    const size_t pushed = conn->incoming.push(buf, static_cast<size_t>(rv));
    assert(pushed == static_cast<size_t>(rv));

    process_incoming_requests(conn);
    drain_outgoing(conn);
}

Conn* Server::accept_connection() {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    const int connfd = accept(listen_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
    if (connfd < 0) {
        return nullptr;
    }

    const uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n", ip & 255, (ip >> 8) & 255,
            (ip >> 16) & 255, ip >> 24, ntohs(client_addr.sin_port));

    fd_set_nb(connfd);
    Conn* conn = new Conn();
    conn->fd = connfd;
    update_io_interest(conn);
    return conn;
}

void Server::close_connection(int fd) {
    if (fd < 0 || static_cast<size_t>(fd) >= fd2conn_.size()) {
        return;
    }

    Conn* conn = fd2conn_[fd];
    if (!conn) {
        return;
    }

    close(conn->fd);
    delete conn;
    fd2conn_[fd] = nullptr;
}

void Server::cleanup_connections() {
    for (Conn*& conn : fd2conn_) {
        if (conn) {
            close(conn->fd);
            delete conn;
            conn = nullptr;
        }
    }
    fd2conn_.clear();
}

void Server::run() {
    while (true) {
        poll_args_.clear();
        poll_args_.push_back({listen_fd_, POLLIN, 0});

        for (Conn* c : fd2conn_) {
            if (!c) {
                continue;
            }

            struct pollfd pfd = {c->fd, POLLERR, 0};
            if (c->want_read) {
                pfd.events |= POLLIN;
            }
            if (c->want_write) {
                pfd.events |= POLLOUT;
            }
            if (c->want_close) {
                pfd.events |= POLLHUP;
            }
            poll_args_.push_back(pfd);
        }

        const int rv = poll(poll_args_.data(), poll_args_.size(), k_poll_timeout_ms);
        if (rv < 0 && errno == EINTR) {
            continue;
        }
        if (rv < 0) {
            msg_errno("poll() error, exiting the event loop");
            break;
        }

        if (poll_args_[0].revents) {
            if (Conn* conn = accept_connection()) {
                if (fd2conn_.size() <= static_cast<size_t>(conn->fd)) {
                    fd2conn_.resize(conn->fd + 1);
                }
                fd2conn_[conn->fd] = conn;
            }
        }

        for (size_t i = 1; i < poll_args_.size(); ++i) {
            struct pollfd* pfd = &poll_args_[i];
            if (pfd->fd >= static_cast<int>(fd2conn_.size())) {
                continue;
            }

            Conn* conn = fd2conn_[pfd->fd];
            if (!conn) {
                continue;
            }

            if (pfd->revents & POLLOUT) {
                assert(conn->want_write);
                drain_outgoing(conn);
            }
            if (pfd->revents & POLLIN) {
                assert(conn->want_read);
                handle_read(conn);
            }

            if ((pfd->revents & POLLERR) || conn->want_close) {
                close_connection(pfd->fd);
            }
        }
    }
}

int main() {
    Server::getInstance(/*max_cache_entries=*/1024).run();
    return 0;
}
