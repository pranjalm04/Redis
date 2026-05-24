#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include "server/ring_buffer.h"

static const size_t k_conn_buf_cap = 256 * 1024;
static const size_t k_max_msg = k_conn_buf_cap - 4;
static const size_t k_max_requests_per_wake = 32;

enum class BlockReason { None, NeedOutgoingSpace };

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    BlockReason blocked = BlockReason::None;
    RingBuffer incoming{k_conn_buf_cap};
    RingBuffer outgoing{k_conn_buf_cap};
};

#endif
