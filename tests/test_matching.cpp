
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


TEST_CASE("no match when bid is below ask") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1));
    ob.add_order(make_order(2, Side::Sell, 101, 10, 2));

    CHECK(ob.order_count() == 2);
    CHECK(ob.best_bid() == 100);
    CHECK(ob.best_ask() == 101);
    CHECK(ob.order_volume(1) == 10);
    CHECK(ob.order_volume(2) == 10);
}


TEST_CASE("exact match removes both orders") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1));
    ob.add_order(make_order(2, Side::Sell, 100, 10, 2));

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.best_bid().has_value());
    CHECK_FALSE(ob.best_ask().has_value());
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
}


TEST_CASE("aggressive order crosses the spread") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Sell, 100, 10, 1));
    ob.add_order(make_order(2, Side::Buy,  101, 10, 2));

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
}


TEST_CASE("partial fill, incoming order is larger") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 5,  1));
    ob.add_order(make_order(2, Side::Sell, 100, 10, 2));

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK(ob.order_volume(2) == 5);
    CHECK(ob.best_ask() == 100);
    CHECK_FALSE(ob.best_bid().has_value());
}


TEST_CASE("partial fill, resting order is larger") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1));
    ob.add_order(make_order(2, Side::Sell, 100, 4,  2));

    CHECK(ob.order_count() == 1);
    CHECK(ob.order_volume(1) == 6);
    CHECK_FALSE(ob.order_volume(2).has_value());
    CHECK(ob.best_bid() == 100);
    CHECK_FALSE(ob.best_ask().has_value());
}


TEST_CASE("time priority, older order at the same price goes first") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 100, 5, 2));

    ob.add_order(make_order(3, Side::Sell, 100, 5, 3));

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK(ob.order_volume(2) == 5);
    CHECK_FALSE(ob.order_volume(3).has_value());
}


TEST_CASE("price priority, highest bid goes first") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 101, 5, 2));

    ob.add_order(make_order(3, Side::Sell, 100, 5, 3));

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(2).has_value());
    CHECK(ob.order_volume(1) == 5);
    CHECK(ob.best_bid() == 100);
}


TEST_CASE("price priority, lowest ask goes first") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Sell, 101, 5, 1));
    ob.add_order(make_order(2, Side::Sell, 100, 5, 2));

    ob.add_order(make_order(3, Side::Buy, 101, 5, 3));

    CHECK(ob.order_count() == 1);
    CHECK_FALSE(ob.order_volume(2).has_value());
    CHECK(ob.order_volume(1) == 5);
    CHECK(ob.best_ask() == 101);
}

TEST_CASE("sweep across multiple resting orders") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 100, 5, 2));

    ob.add_order(make_order(3, Side::Sell, 100, 10, 3));

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
    CHECK_FALSE(ob.order_volume(3).has_value());
}


TEST_CASE("cancel of an unknown id returns false") {
    OrderBook ob;
    CHECK_FALSE(ob.cancel_order(999));

    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    CHECK_FALSE(ob.cancel_order(2));
    CHECK(ob.order_count() == 1);
}


TEST_CASE("cancel removes an order exactly once") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));

    CHECK(ob.cancel_order(1));
    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.cancel_order(1));
}


TEST_CASE("cancel cleans up an empty price level") {
    OrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 5, 1));
    ob.add_order(make_order(2, Side::Buy, 101, 5, 2));

    CHECK(ob.best_bid() == 101);
    CHECK(ob.cancel_order(2));
    CHECK(ob.best_bid() == 100);
    CHECK(ob.order_count() == 1);
}
