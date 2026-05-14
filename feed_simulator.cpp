#pragma once;
#include <cstdlib>
#include "order_book.h"
#include <chrono>

void generateX(OrderBook& ob, int count, Price base_price, Volume base_volume) {
    while (count > 0) {
        bool is_ask = (rand() % 100) >= 50;

        int offset = (rand() % 11) - 5; 
        Price price = base_price + offset; 

        Volume volume = base_volume + (rand() % 10); 

        uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();          

        Side side = is_ask ? Side::Sell : Side::Buy;
        Order order{ (OrderId)count, price, volume, side, timestamp };

        ob.add_order(order);
        --count;
    }
}






