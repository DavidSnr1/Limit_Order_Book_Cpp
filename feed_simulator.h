#pragma once
#include <cstdlib>
#include "order_book.h"
#include "order.h"
#include <chrono>

Order generateX(OrderId id, Price base_price, Volume base_volume);
