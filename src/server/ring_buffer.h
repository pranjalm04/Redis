#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

// Fixed-capacity byte ring buffer.
// tail_ = read index, head_ = write index.
// One slot is unused so empty (head == tail) and full are distinguishable.
class RingBuffer {
  public:
    explicit RingBuffer(size_t capacity);

    // Append bytes. Returns how many bytes were written (0 if full).
    size_t push(const uint8_t* data, size_t len);

    // Copy bytes from the front into dst. Returns how many bytes were copied.
    size_t pop(uint8_t* dst, size_t len);

    // Copy bytes at logical offset from the front without consuming.
    size_t peek(size_t offset, uint8_t* dst, size_t len) const;

    // Discard len bytes from the front. len must be <= used().
    void consume(size_t len);

    // First contiguous readable span (for writev/write).
    const uint8_t* read_ptr() const;
    size_t readable_contiguous() const;

    bool empty() const;
    bool full() const;
    size_t used() const;
    size_t free_space() const;
    size_t capacity() const;

  private:
    size_t physical_size() const { return buffer_.size(); }

    std::vector<uint8_t> buffer_;
    size_t head_;
    size_t tail_;
};

#endif
