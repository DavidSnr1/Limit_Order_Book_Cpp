#pragma once
#include "order.h"


struct Trade {
    Price price; 
    Volume qty; 
    OrderId buy_id;
    OrderId sell_id; 
};