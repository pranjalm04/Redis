#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <cstdint>
#include <assert.h>

const size_t k_max_msg = 32 << 20;

void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

struct Conn{
    int fd;
    bool want_read;
    bool want_write;
    bool want_close;
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};

static void msg_errno(const char *msg){
    fprintf(stderr, "[%d] %s\n", errno, msg);
}
void fd_set_nb(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags<0) {
        die("fcntl(F_GETFL)");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    int f = fcntl(fd, F_SETFL, flags);
    if (f<0) {
        die("fcntl(F_SETFL)");
        return;
    }
}

Conn* handle_accept(int fd)
{
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if(connfd < 0)
    {
        return NULL;
    }
    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
        ntohs(client_addr.sin_port)
    );
    fd_set_nb(connfd);
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

static void buf_consume(std::vector<uint8_t>& buf, size_t len)
{
    buf.erase(buf.begin(), buf.begin() + len);
}

bool try_one_request(Conn* conn)
{
    if(conn->incoming.size() < 4)
    {
        return false;
    }

    uint32_t len_be = 0;
    memcpy(&len_be, conn->incoming.data(), 4);
    uint32_t len = ntohl(len_be);
    if(len > k_max_msg)
    {
        msg_errno("client sent too long message");
        conn->want_close = true;
        return false;
    }
    if(conn->incoming.size() < 4 + len)
    {
        return false;
    }
    const uint8_t* request = &conn->incoming[4];
    printf("client says: len:%d data:%.*s\n",
        len, len < 100 ? len : 100, request);

    uint32_t out_len_be = htonl(len);
    conn->outgoing.insert(
        conn->outgoing.end(),
        reinterpret_cast<const uint8_t*>(&out_len_be),
        reinterpret_cast<const uint8_t*>(&out_len_be) + sizeof(out_len_be));
    conn->outgoing.insert(conn->outgoing.end(),request, request + len);
    buf_consume(conn->incoming, 4 + len);
    conn->want_write = true;
    return true;

    
}
static void handle_write(Conn *conn)
{
    assert(conn->outgoing.size()>0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if(rv<0 && errno == EAGAIN)
    {
        return;
    }
    if(rv < 0)
    {
        die("write");
        conn->want_close = true;
        return;
    }
    buf_consume(conn->outgoing, (size_t)rv);

    if(conn->outgoing.empty())
    {
        conn->want_write = false;
        conn->want_read = true;
    }
}
static void handle_read(Conn *conn)
{
    uint8_t buf[64*1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    
    if(rv<0 && errno == EAGAIN)
    {
        return;
    }
    if(rv < 0)
    {
        die("read");
    }
    if(rv == 0)
    {
        if(conn->incoming.empty())
        {
            msg_errno("client disconnected before sending anything");
        }
        else
        {
            msg_errno("unexpected EOF");
        }

        conn->want_close = true;
    }
    conn->incoming.insert(conn->incoming.end(), buf, buf + (size_t)rv);

    while(try_one_request(conn)){}

    if(conn->outgoing.size())
    {
        conn->want_write = true;
        conn->want_read = false;
        handle_write(conn);
    }
}
int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_ANY);

    int rv = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if(rv < 0)
    {
        die("bind");
    }

    rv = listen(fd, SOMAXCONN);
    if(rv < 0)
    {
        die("listen");
    }

    std::vector<Conn *> fd2conn;
    std::vector<struct pollfd> poll_args;

    while(true){
        poll_args.clear();
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for(Conn *c : fd2conn){
            if(!c) 
            {
                continue;
            }

            struct pollfd pfd = {c->fd, POLLERR, 0};
            if(c->want_read){
                pfd.events |= POLLIN;
            }
            if(c->want_write){
                pfd.events |= POLLOUT;
            }
            if(c->want_close){
                pfd.events |= POLLHUP;
            }
            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), poll_args.size(), 1000);
        if(rv < 0 && errno == EINTR)
        {
            continue;
        }
        if(rv < 0)
        {
            die("poll");
        }

        if(poll_args[0].revents)
        {
            if(Conn *conn = handle_accept(fd))
            {
                if(fd2conn.size() <= (size_t)conn->fd)
                {
                    fd2conn.resize(conn->fd+1);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        for(int i = 1; i < poll_args.size(); i++)
        {
            struct pollfd *pfd = &poll_args[i];
            if(pfd->fd >= fd2conn.size())
            {
                continue;
            }
            Conn *conn = fd2conn[pfd->fd];
            
            if(!conn)
            {
                continue;
            }

            if(pfd->revents & POLLIN)
            {
                assert(conn->want_read);
                handle_read(conn);
            }
            if(pfd->revents & POLLOUT)
            {
                assert(conn->want_write);
                handle_write(conn);
            }
           
            if((pfd->revents & POLLERR) || conn->want_close)
            {
                close(conn->fd);
                delete conn;
                fd2conn[pfd->fd] = nullptr;
                continue;
            }
        }

    }
    return 0;
}