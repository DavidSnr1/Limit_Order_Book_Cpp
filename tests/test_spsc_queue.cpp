// built together with test_matching.cpp (that file has DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN),
// this file only contributes more TEST_CASEs.
#include "doctest.h"

#include "../spsc_queue.h"
#include <thread>
#include <vector>


TEST_CASE("push then pop returns the same value") {
    SPSCQueue<int, 4> q;
    CHECK(q.push(42));

    int out = 0;
    CHECK(q.pop(out));
    CHECK(out == 42);
}

TEST_CASE("pop on an empty queue returns false") {
    SPSCQueue<int, 4> q;
    int out = 0;
    CHECK_FALSE(q.pop(out));
}

TEST_CASE("push preserves FIFO order") {
    SPSCQueue<int, 4> q;
    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));

    int out = 0;
    CHECK(q.pop(out)); CHECK(out == 1);
    CHECK(q.pop(out)); CHECK(out == 2);
    CHECK(q.pop(out)); CHECK(out == 3);
}

TEST_CASE("push fails once the queue is full") {
    // capacity 4 holds only 3 usable slots: one slot always stays empty,
    // otherwise a full queue (head == tail) would look identical to an
    // empty one.
    SPSCQueue<int, 4> q;
    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));
    CHECK_FALSE(q.push(4));
}

TEST_CASE("popping frees a slot for the next push") {
    SPSCQueue<int, 4> q;
    q.push(1); q.push(2); q.push(3);

    int out = 0;
    CHECK(q.pop(out));
    CHECK(q.push(4));

    CHECK(q.pop(out)); CHECK(out == 2);
    CHECK(q.pop(out)); CHECK(out == 3);
    CHECK(q.pop(out)); CHECK(out == 4);
}

TEST_CASE("indices wrap around past the end of the buffer") {
    SPSCQueue<int, 4> q;
    // push/pop one at a time, many more times than the capacity, so head_
    // and tail_ wrap past Capacity - 1 back to 0 repeatedly.
    for (int i = 0; i < 100; i++) {
        CHECK(q.push(i));
        int out = -1;
        CHECK(q.pop(out));
        CHECK(out == i);
    }
}

TEST_CASE("two threads: producer pushes 0..N-1, consumer receives them in order") {
    SPSCQueue<int, 256> q;
    constexpr int N = 20000;
    std::vector<int> received;
    received.reserve(N);

    std::thread producer([&] {
        for (int i = 0; i < N; i++) {
            while (!q.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        int value;
        int count = 0;
        while (count < N) {
            if (q.pop(value)) {
                received.push_back(value);
                count++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // asserting from a background thread would need doctest's own locking,
    // so all checks happen here in the main test thread after both threads
    // have joined.
    REQUIRE(received.size() == N);
    for (int i = 0; i < N; i++) {
        CHECK(received[i] == i);
    }
}
