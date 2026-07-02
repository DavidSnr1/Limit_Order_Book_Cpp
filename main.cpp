#include "feed_simulator.h"
#include "order_book.h"

int main() {
    OrderBook ob = OrderBook();

    int count = 10;

    for (int i = 0; i < count; i++){
        ob.add_order(generateX(i,100,100));
        ob.display();
    }


}
