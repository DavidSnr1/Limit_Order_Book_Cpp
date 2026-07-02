#pragma once
#include "order.h"
#include "types.h"
#include "trade.h"
#include <vector>
#include <map>
#include <list>
#include <unordered_map>
#include <functional>
#include <optional>
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

    using OrderList = std::list<Order>;
    using OrderIt = OrderList::iterator;

    struct OrderLocation {
        Price price;
        Side side;
        OrderIt it;
    };

    std::map<Price, std::list<Order>, std::greater<Price>> bids;
    std::map<Price, std::list<Order>> asks;
    std::vector<Trade> trade_log;
    std::unordered_map<OrderId, OrderLocation> order_map;
};