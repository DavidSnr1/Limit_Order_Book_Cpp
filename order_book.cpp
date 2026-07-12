#include "order_book.h"

bool OrderBook::add_order(Order order){
    if (order.side == Side::Buy){
        auto& list = bids[order.price];
        list.push_back(order);
        OrderIt it = std::prev(list.end());
        order_map[order.id] = {order.price, Side::Buy, it};
    }
    else if (order.side == Side::Sell){
        auto& list = asks[order.price];
        list.push_back(order);
        OrderIt it = std::prev(list.end());
        order_map[order.id] = {order.price, Side::Sell, it};
    }
    match();
    return true;
}

bool OrderBook::cancel_order(OrderId o_id){
    auto itmap = order_map.find(o_id);
    if (itmap == order_map.end()) return false;
    
    auto& location = itmap->second;


    if (location.side == Side::Buy){
        bids[location.price].erase(location.it);
        if (bids[location.price].empty()) bids.erase(location.price);
    }
    else {
        asks[location.price].erase(location.it);
        if (asks[location.price].empty()) asks.erase(location.price);
    }
    order_map.erase(itmap);
    return true;    
}

void OrderBook::match() {
    while (!bids.empty() && !asks.empty()) {
        auto bid_it = bids.begin();
        auto ask_it = asks.begin();
        if (bid_it->first < ask_it->first) break;

        auto& bid_queue = bid_it->second;
        auto& ask_queue = ask_it->second;
        Order& b = bid_queue.front();
        Order& a = ask_queue.front();

        Volume trade_vol = std::min(b.volume, a.volume);
        b.volume -= trade_vol;
        a.volume -= trade_vol;

        OrderId a_id = a.id;
        OrderId b_id = b.id;

        Order resting_o;
        a.timestamp < b.timestamp ? resting_o = a : resting_o = b;

        Trade trade = {
            resting_o.price,
            trade_vol,
            b_id,
            a_id
        };

        trade_log.push_back(trade);

        std::cout << "Trade @ Quantity " << trade.qty << " @ Price: " << trade.price << "\n";

        if (b.volume == 0) {
            order_map.erase(b_id);
            bid_queue.pop_front();
            if (bid_queue.empty()) bids.erase(bid_it);
        }
        if (a.volume == 0) {
            order_map.erase(a_id);
            ask_queue.pop_front();
            if (ask_queue.empty()) asks.erase(ask_it);
        }
    }
}

bool OrderBook::display (int depth){
    std::cout << "--- BID ---" << std::endl;
    int d = 0;
    for (const auto& [price, list] : bids){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (const auto& order : list){
            std::cout << " [ " << order.volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }

    std::cout << "--- ASK ---" << std::endl;
    d=0;

    for (const auto& [price, list] : asks){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (const auto& order : list){
            std::cout << " [ " << order.volume << " ] ";
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
    return it->second.it->volume;
}

std::size_t OrderBook::order_count() const {
    return order_map.size();
}





