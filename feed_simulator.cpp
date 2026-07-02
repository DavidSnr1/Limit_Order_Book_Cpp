#include "feed_simulator.h"

Order generateX(OrderId id, Price base_price, Volume base_volume) {

    bool is_ask = (rand() % 100) >= 50;

    int offset = (rand() % 11) - 5; 
    Price price = base_price + offset; 

    Volume volume = base_volume + (rand() % 10); 

    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();          

    Side side = is_ask ? Side::Sell : Side::Buy;

    Order order = Order{id, price, volume, side, timestamp};

    return order;
}




