#pragma once
#include <array>
#include <atomic>
#include <cstddef>

// Bounded lock-free ring buffer for exactly one producer thread and exactly
// one consumer thread. Concurrent push()+push() or pop()+pop() from more
// than one thread each is undefined behaviour -- the whole design relies on
// there being a single writer for tail_/head_cached_ and a single writer
// for head_/tail_cached_.
template <typename T, std::size_t Capacity>
class SPSCQueue {
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool push(const T& item) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = advance(tail);

        // head_cached_ is our own private guess at head_ from last time --
        // re-reading the real (shared, cross-core) head_ on every single
        // push would defeat the point of splitting producer/consumer state
        // onto separate cache lines below. Only pay for the atomic load
        // when the cached guess says the queue might actually be full.
        if (next == head_cached_) {
            head_cached_ = head_.load(std::memory_order_acquire);
            if (next == head_cached_) return false;
        }

        buffer_[tail] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        const std::size_t head = head_.load(std::memory_order_relaxed);

        if (head == tail_cached_) {
            tail_cached_ = tail_.load(std::memory_order_acquire);
            if (head == tail_cached_) return false;
        }

        out = buffer_[head];
        head_.store(advance(head), std::memory_order_release);
        return true;
    }

private:
    static std::size_t advance(std::size_t idx) {
        return (idx + 1) & (Capacity - 1);
    }

    std::array<T, Capacity> buffer_{};

    // Grouped by which thread writes them, not by which index they track:
    // head_ and tail_cached_ are both only ever written by the consumer,
    // tail_ and head_cached_ are both only ever written by the producer.
    // alignas(64) pins each group to its own cache line so a push() on one
    // core never invalidates the cache line the other core is reading --
    // otherwise the two threads would ping-pong the same cache line back
    // and forth on every single operation (false sharing), which is slower
    // than the mutex this class exists to avoid.
    alignas(64) std::atomic<std::size_t> head_{0};
    std::size_t tail_cached_{0};

    alignas(64) std::atomic<std::size_t> tail_{0};
    std::size_t head_cached_{0};
};
