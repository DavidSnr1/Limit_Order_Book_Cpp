#pragma once
#include "order.h"
#include "types.h"
#include "trade.h"
#include "memory_pool.h"
#include <vector>
#include <map>
#include <list>
#include <unordered_map>
#include <functional>
#include <optional>
#include <iostream>
#include <cstddef>


class OrderBook {
    public:

    bool add_order(Order order);
    bool cancel_order(OrderId id);
    bool display(int depth = 5);


    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

    std::optional<Volume> order_volume(OrderId id) const;

    std::size_t order_count() const;

    private:

    void match();

    struct OrderQueue {
        Order* head = nullptr;
        Order* tail = nullptr;
    };

    struct OrderLocation {
        Price price;
        Side side;
        Order* order;
    };

    MemoryPool order_pool_{sizeof(Order), 1024}; 
    std::map<Price, OrderQueue, std::greater<Price>> bids;
    std::map<Price, OrderQueue> asks;
    std::vector<Trade> trade_log;
    std::unordered_map<OrderId, OrderLocation> order_map;
};