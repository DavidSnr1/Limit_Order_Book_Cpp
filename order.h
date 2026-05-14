#pragma once;
#include "types.h";

struct Order {
    OrderId id;
    Price price;
    Volume volume;
    Side side;
    uint64_t timestamp;
};