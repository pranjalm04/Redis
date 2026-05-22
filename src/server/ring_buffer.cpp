#include "server/ring_buffer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

RingBuffer::RingBuffer(size_t capacity)
    : buffer_(capacity > 0 ? capacity + 1 : 1), head_(0), tail_(0) {}

size_t RingBuffer::push(const uint8_t* data, size_t len) {
    if (len == 0 || free_space() == 0) {
        return 0;
    }

    const size_t n = physical_size();
    const size_t to_write = std::min(len, free_space());
    size_t written = 0;

    while (written < to_write) {
        const size_t chunk =
            std::min(to_write - written, n - head_);
        std::memcpy(&buffer_[head_], data + written, chunk);
        head_ = (head_ + chunk) % n;
        written += chunk;
    }

    return written;
}

size_t RingBuffer::pop(uint8_t* dst, size_t len) {
    if (len == 0 || empty()) {
        return 0;
    }

    const size_t n = physical_size();
    const size_t to_read = std::min(len, used());
    size_t read = 0;

    while (read < to_read) {
        const size_t chunk = std::min(to_read - read, n - tail_);
        std::memcpy(dst + read, &buffer_[tail_], chunk);
        tail_ = (tail_ + chunk) % n;
        read += chunk;
    }

    return read;
}

size_t RingBuffer::peek(size_t offset, uint8_t* dst, size_t len) const {
    const size_t avail = used();
    if (len == 0 || offset >= avail) {
        return 0;
    }

    const size_t n = physical_size();
    const size_t to_copy = std::min(len, avail - offset);
    size_t pos = (tail_ + offset) % n;
    size_t copied = 0;

    while (copied < to_copy) {
        const size_t chunk = std::min(to_copy - copied, n - pos);
        std::memcpy(dst + copied, &buffer_[pos], chunk);
        pos = (pos + chunk) % n;
        copied += chunk;
    }

    return copied;
}

void RingBuffer::consume(size_t len) {
    assert(len <= used());
    const size_t n = physical_size();
    tail_ = (tail_ + len) % n;
}

const uint8_t* RingBuffer::read_ptr() const {
    return empty() ? nullptr : &buffer_[tail_];
}

size_t RingBuffer::readable_contiguous() const {
    if (empty()) {
        return 0;
    }
    if (head_ > tail_) {
        return head_ - tail_;
    }
    return physical_size() - tail_;
}

bool RingBuffer::empty() const { return head_ == tail_; }

bool RingBuffer::full() const {
    const size_t n = physical_size();
    return n > 0 && head_ == (tail_ + n - 1) % n;
}

size_t RingBuffer::used() const {
    const size_t n = physical_size();
    if (n == 0) {
        return 0;
    }
    if (head_ >= tail_) {
        return head_ - tail_;
    }
    return n - tail_ + head_;
}

size_t RingBuffer::free_space() const { return capacity() - used(); }

size_t RingBuffer::capacity() const {
    const size_t n = physical_size();
    return n > 0 ? n - 1 : 0;
}
