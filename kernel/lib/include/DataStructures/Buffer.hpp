/*
Copyright (©) 2023-2026  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _BUFFER_HPP
#define _BUFFER_HPP

#include <atomic>
#include <cstdint>
#include <spinlock.h>

#include "LinkedList.hpp"

#define DEFAULT_BUFFER_BLOCK_SIZE 256

// A dynamic buffer created from multiple blocks
class Buffer {
public:
    Buffer();
    explicit Buffer(size_t size, size_t blockSize = DEFAULT_BUFFER_BLOCK_SIZE);
    virtual ~Buffer();

    // Write size bytes from data to the buffer at offset
    virtual void Write(uint64_t offset, const uint8_t* data, size_t size);

    // Read size bytes from the buffer at offset to data
    virtual void Read(uint64_t offset, uint8_t* data, size_t size) const;

    // Clear size bytes starting at offset. Potentially could remove the block if it is empty
    void Clear(uint64_t offset, size_t size);

    // Clear the buffer
    void Clear();

    // Remove any unused blocks at the end
    void AutoShrink();

    // Clear the buffer until offset. Will delete any empty blocks. Returns the number of blocks deleted.
    uint64_t ClearUntil(uint64_t offset);

    // Get the size of the buffer
    size_t GetSize() const;

protected:
    struct Block {
        uint8_t* data;
        size_t size;
        bool empty;
    };

    virtual Block* AddBlock(size_t size);
    virtual void DeleteBlock(uint64_t index);

private:
    size_t m_size;
    size_t m_blockSize;
    LinkedList::RearInsertLinkedList<Block> m_blocks;
};

class StreamBuffer {
public:
    StreamBuffer() = default;
    virtual ~StreamBuffer() = default;

    virtual void WriteStream(const uint8_t* data, size_t size) = 0;
    virtual void ReadStream(uint8_t* data, size_t size) const = 0;

    virtual void WriteStream8(uint8_t data);
    virtual void ReadStream8(uint8_t& data);
    virtual void WriteStream16(uint16_t data);
    virtual void ReadStream16(uint16_t& data);
    virtual void WriteStream32(uint32_t data);
    virtual void ReadStream32(uint32_t& data);
    virtual void WriteStream64(uint64_t data);
    virtual void ReadStream64(uint64_t& data);

    virtual void SeekStream(uint64_t offset) = 0;

    [[nodiscard]] virtual uint64_t GetOffset() const = 0;
};

template <typename T, size_t Capacity>
class RingBuffer { // Single-Producer, Multi-Consumer design
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity > 0, "Capacity must be greater than 0");

private:
    T buffer_[Capacity];

    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    spinlock_t consumer_lock_;

    static constexpr size_t Mask = Capacity - 1;

public:
    RingBuffer() {
        spinlock_init(&consumer_lock_);
    }

    ~RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    bool push(const T& item) {
        const size_t currentHead = head_.load(std::memory_order_relaxed);
        const size_t currentTail = head_.load(std::memory_order_acquire);

        if (currentHead - currentTail >= Capacity)
            return false; // Buffer is full

        buffer_[currentHead & Mask] = item;
        head_.store(currentHead + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        spinlock_acquire(&consumer_lock_);

        const size_t currentTail = tail_.load(std::memory_order_relaxed);
        const size_t currentHead = head_.load(std::memory_order_acquire);

        if (currentTail == currentHead) {
            spinlock_release(&consumer_lock_);
            return false; // Buffer is empty
        }

        item = buffer_[currentTail & Mask];
        tail_.store(currentTail + 1, std::memory_order_release);

        spinlock_release(&consumer_lock_);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    bool full() const {
        return (head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed)) >= Capacity;
    }

    size_t size() const {
        return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
    }

    constexpr size_t capacity() const {
        return Capacity;
    }
};

#endif /* _BUFFER_HPP */