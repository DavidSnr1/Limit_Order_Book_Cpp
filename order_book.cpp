#include "order_book.h"
#include <new>

bool OrderBook::add_order(Order order){
    if (order.side == Side::Buy){
        auto& queue = bids[order.price];

        void* raw = order_pool_.allocate();
        Order* new_order = new (raw) Order(order);

        if (queue.tail == nullptr) {
            queue.head = new_order;
            queue.tail = new_order;
        } else {
            queue.tail->next_order = new_order;
            new_order->prev_order = queue.tail;
            queue.tail = new_order;
        }

        order_map[order.id] = {order.price, Side::Buy, new_order};
    }
    else if (order.side == Side::Sell){
        auto& queue = asks[order.price];

        void* raw = order_pool_.allocate();
        Order* new_order = new (raw) Order(order);

        if (queue.tail == nullptr) {
            queue.head = new_order;
            queue.tail = new_order;
        } else {
            queue.tail->next_order = new_order;
            new_order->prev_order = queue.tail;
            queue.tail = new_order;
        }

        order_map[order.id] = {order.price, Side::Sell, new_order};
    }
    match();
    return true;
}

bool OrderBook::cancel_order(OrderId o_id){
    auto it = order_map.find(o_id);
    if (it == order_map.end()) return false;

    OrderLocation& location = it->second;
    Order* order = location.order;

    if (location.side == Side::Buy) {
        auto priceIt = bids.find(location.price);
        OrderQueue& queue = priceIt->second;

        if (order->prev_order == nullptr && order->next_order == nullptr) {
            queue.head = nullptr;
            queue.tail = nullptr;
        }
        else if (order->prev_order == nullptr) {
            queue.head = order->next_order;
            queue.head->prev_order = nullptr;
        }
        else if (order->next_order == nullptr) {
            queue.tail = order->prev_order;
            queue.tail->next_order = nullptr;
        }
        else {
            order->prev_order->next_order = order->next_order;
            order->next_order->prev_order = order->prev_order;
        }

        if (queue.head == nullptr) {
            bids.erase(priceIt);
        }
    }
    else {
        auto priceIt = asks.find(location.price);
        OrderQueue& queue = priceIt->second;

        if (order->prev_order == nullptr && order->next_order == nullptr) {
            queue.head = nullptr;
            queue.tail = nullptr;
        }
        else if (order->prev_order == nullptr) {
            queue.head = order->next_order;
            queue.head->prev_order = nullptr;
        }
        else if (order->next_order == nullptr) {
            queue.tail = order->prev_order;
            queue.tail->next_order = nullptr;
        }
        else {
            order->prev_order->next_order = order->next_order;
            order->next_order->prev_order = order->prev_order;
        }

        if (queue.head == nullptr) {
            asks.erase(priceIt);
        }
    }

    order->~Order();
    order_pool_.deallocate(order);

    order_map.erase(it);
    return true;
}

void OrderBook::match() {
    while (!bids.empty() && !asks.empty()) {
        auto bid_it = bids.begin();
        auto ask_it = asks.begin();
        if (bid_it->first < ask_it->first) break;

        auto& bid_queue = bid_it->second;
        auto& ask_queue = ask_it->second;
        Order* b = bid_queue.head;
        Order* a = ask_queue.head;

        Volume trade_vol = std::min(b->volume, a->volume);
        b->volume -= trade_vol;
        a->volume -= trade_vol;

        OrderId a_id = a->id;
        OrderId b_id = b->id;

        Order* resting_o = (a->timestamp < b->timestamp) ? a : b;

        Trade trade = {
            resting_o->price,
            trade_vol,
            b_id,
            a_id
        };

        trade_log.push_back(trade);

        std::cout << "Trade @ Quantity " << trade.qty << " @ Price: " << trade.price << "\n";

        if (b->volume == 0) {
            order_map.erase(b_id);

            bid_queue.head = b->next_order;
            if (bid_queue.head) {
                bid_queue.head->prev_order = nullptr;
            } else {
                bid_queue.tail = nullptr;
            }

            b->~Order();
            order_pool_.deallocate(b);

            if (bid_queue.head == nullptr) bids.erase(bid_it);
        }
        if (a->volume == 0) {
            order_map.erase(a_id);

            ask_queue.head = a->next_order;
            if (ask_queue.head) {
                ask_queue.head->prev_order = nullptr;
            } else {
                ask_queue.tail = nullptr;
            }

            a->~Order();
            order_pool_.deallocate(a);

            if (ask_queue.head == nullptr) asks.erase(ask_it);
        }
    }
}

bool OrderBook::display (int depth){
    std::cout << "--- BID ---" << std::endl;
    int d = 0;
    for (const auto& [price, queue] : bids){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (Order* cur = queue.head; cur; cur = cur->next_order){
            std::cout << " [ " << cur->volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }

    std::cout << "--- ASK ---" << std::endl;
    d=0;

    for (const auto& [price, queue] : asks){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (Order* cur = queue.head; cur; cur = cur->next_order){
            std::cout << " [ " << cur->volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }
    std::cout << std::endl;
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids.empty()) return std::nullopt;
    return bids.begin()->first;   
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks.empty()) return std::nullopt;
    return asks.begin()->first;   
}

std::optional<Volume> OrderBook::order_volume(OrderId id) const {
    auto it = order_map.find(id);
    if (it == order_map.end()) return std::nullopt;
    return it->second.order->volume;
}

std::size_t OrderBook::order_count() const {
    return order_map.size();
}







