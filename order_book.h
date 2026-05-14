#pragma once;
#include "order.h";
#include "types.h";
#include <map>
#include <list>
#include <unordered_map>

class OrderBook {
    public:

    bool add_order(Order order);
    bool cancel_order(OrderId id);
    bool display(int depth = 5);

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
    std::unordered_map<OrderId, OrderLocation> order_map;
};