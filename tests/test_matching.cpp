
// build (g++):    g++ -std=c++20 -O2 tests/test_matching.cpp order_book.cpp -o test

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../order_book.h"
#include "../order.h"
#include "../types.h"


static Order make_order(OrderId id, Side side, Price price, Volume volume,
                        uint64_t timestamp = 0) {
    return Order{ id, price, volume, side, timestamp };
}


TEST_CASE("kein Match wenn bid unter ask liegt") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1));
    ob.add_order(make_order(2, Side::Sell, 101, 10, 2));

    CHECK(ob.order_count() == 2);
    CHECK(ob.best_bid() == 100);
    CHECK(ob.best_ask() == 101);
    CHECK(ob.order_volume(1) == 10);
    CHECK(ob.order_volume(2) == 10);
}


TEST_CASE("exakter Match loescht beide Orders") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1));
    ob.add_order(make_order(2, Side::Sell, 100, 10, 2));

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.best_bid().has_value());
    CHECK_FALSE(ob.best_ask().has_value());
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
}


TEST_CASE("aggressive Order crosst den Spread") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Sell, 100, 10, 1)); 
    ob.add_order(make_order(2, Side::Buy,  101, 10, 2)); 

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
}


TEST_CASE("Teilfuellung - eingehende Order ist groesser") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 5,  1)); 
    ob.add_order(make_order(2, Side::Sell, 100, 10, 2));  

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(1).has_value()); 
    CHECK(ob.order_volume(2) == 5);              
    CHECK(ob.best_ask() == 100);                 
    CHECK_FALSE(ob.best_bid().has_value());
}


TEST_CASE("Teilfuellung - ruhende Order ist groesser") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1)); 
    ob.add_order(make_order(2, Side::Sell, 100, 4,  2)); 

    CHECK(ob.order_count() == 1);
    CHECK(ob.order_volume(1) == 6);              
    CHECK_FALSE(ob.order_volume(2).has_value()); 
    CHECK(ob.best_bid() == 100);
    CHECK_FALSE(ob.best_ask().has_value());
}


TEST_CASE("Zeitprioritaet - aeltere Order bei gleichem Preis zuerst") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1)); 
    ob.add_order(make_order(2, Side::Buy, 100, 5, 2)); 

    ob.add_order(make_order(3, Side::Sell, 100, 5, 3)); 

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(1).has_value()); 
    CHECK(ob.order_volume(2) == 5);              
    CHECK_FALSE(ob.order_volume(3).has_value()); 
}


TEST_CASE("Preisprioritaet - hoechster Bid zuerst") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 101, 5, 2)); 

    ob.add_order(make_order(3, Side::Sell, 100, 5, 3));

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(2).has_value()); 
    CHECK(ob.order_volume(1) == 5);              
    CHECK(ob.best_bid() == 100);
}


TEST_CASE("Preisprioritaet - niedrigster Ask zuerst") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Sell, 101, 5, 1));
    ob.add_order(make_order(2, Side::Sell, 100, 5, 2)); 

    ob.add_order(make_order(3, Side::Buy, 101, 5, 3));

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(2).has_value()); 
    CHECK(ob.order_volume(1) == 5);              
    CHECK(ob.best_ask() == 101);
}

TEST_CASE("Sweep ueber mehrere ruhende Orders") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 100, 5, 2));

    ob.add_order(make_order(3, Side::Sell, 100, 10, 3)); 

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
    CHECK_FALSE(ob.order_volume(3).has_value());
}


TEST_CASE("Cancel unbekannter ID gibt false zurueck") {
    OrderBook ob;
    CHECK_FALSE(ob.cancel_order(999));

    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    CHECK_FALSE(ob.cancel_order(2)); 
    CHECK(ob.order_count() == 1);   
}


TEST_CASE("Cancel entfernt Order genau einmal") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));

    CHECK(ob.cancel_order(1));                    
    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.cancel_order(1));              
}


TEST_CASE("Cancel raeumt leeres Preislevel ab") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 101, 5, 2));

    CHECK(ob.best_bid() == 101);
    CHECK(ob.cancel_order(2));       
    CHECK(ob.best_bid() == 100);     
    CHECK(ob.order_count() == 1);
}
