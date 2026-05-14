#include "order_book.h"
#include <iostream>

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
        auto& [best_bid_price, bid_queue] = *bids.begin();
        auto& [best_ask_price, ask_queue] = *asks.begin();

        if (best_bid_price < best_ask_price) break; 

        Order& b_ord = bid_queue.front();
        Order& a_ord = ask_queue.front();

        Price match_price = (b_ord.timestamp < a_ord.timestamp) ? best_bid_price : best_ask_price;
        uint32_t match_vol = std::min(b_ord.volume, a_ord.volume);

        b_ord.volume -= match_vol;
        a_ord.volume -= match_vol;

        if (b_ord.volume == 0) {
            order_map.erase(b_ord.id);
            bid_queue.pop_front();
        }
        if (a_ord.volume == 0) {
            order_map.erase(b_ord.id);
            bid_queue.pop_front();
        }
    }
}

bool OrderBook::display (int depth){
    int d = 0;
    for (auto& const [price, list] : bids){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (auto& const order : list){
            std::cout << " [ " << order.volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }

    d=0;

    for (auto& const [price, list] : asks){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (auto& const order : list){
            std::cout << " [ " << order.volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }
    return true;   
}

