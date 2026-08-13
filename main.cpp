#include "feed_simulator.h"
#include "order_book.h"
#include "spsc_queue.h"

#include <thread>

int main() {
    OrderBook ob;
    SPSCQueue<Order, 1024> queue;

    constexpr int count = 10;

    std::thread producer([&] {
        for (int i = 0; i < count; i++) {
            Order order = generateX(i, 100, 100);
            while (!queue.push(order)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        Order order;
        for (int processed = 0; processed < count; ) {
            if (queue.pop(order)) {
                ob.add_order(order);
                ob.display();
                processed++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
}
