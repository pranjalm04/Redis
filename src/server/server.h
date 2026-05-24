#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <cstdint>
#include <vector>

#include "server/connection.h"
#include "server/ring_buffer.h"
#include "server/server_context.h"

class Server {
  public:
    static Server& getInstance(size_t max_cache_entries, uint16_t port = 1234);

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void run();

  private:
    Server(size_t max_cache_entries, uint16_t port);
    ~Server();
    static void die(const char* msg);
    static void msg_errno(const char* msg);
    static void fd_set_nb(int fd);
    static bool push_bytes(RingBuffer& buf, const uint8_t* data, size_t len);

    void update_io_interest(Conn* conn);
    bool try_one_request(Conn* conn);
    void process_incoming_requests(Conn* conn);
    void handle_write(Conn* conn);
    void drain_outgoing(Conn* conn);
    void handle_read(Conn* conn);
    Conn* accept_connection();
    void close_connection(int fd);
    void cleanup_connections();

    ServerContext ctx_;
    uint16_t port_;
    int listen_fd_;
    std::vector<Conn*> fd2conn_;
    std::vector<struct pollfd> poll_args_;
};

#endif
