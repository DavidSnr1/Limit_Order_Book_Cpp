# Limit Order Book

A C++ limit order book with price time priority matching, the core component exchanges (and HFT firms) use to bring buy and sell orders together.

This project is under active development. It is a learning project, not a finished product; see "Current status" below for what is done and what is still open.

## Why this is interesting

In every electronic trading system (NASDAQ, NYSE, crypto exchanges), an order does not meet a counterparty immediately. It waits in a queue, sorted by price, and at the same price by arrival time ("price time priority"). The order book is the data structure that manages these queues and checks after every new order whether a trade can happen. Latency matters in nanoseconds here, so the choice of data structures matters as much as the correctness of the logic.

## Project structure

- `types.h`: base types (`OrderId`, `Price`, `Volume` as `uint64_t`; `Side`, `MessageType`)
- `order.h`: the `Order` struct (including `next_order`/`prev_order` for the intrusive list)
- `trade.h`: the `Trade` struct (result of a match)
- `memory_pool.h` / `memory_pool.cpp`: a pre allocated pool with a free list instead of a heap allocation per order
- `order_book.h` / `order_book.cpp`: the engine, `add_order`, `cancel_order`, `display`, matching with price time priority; price levels are intrusive linked lists whose nodes come from the `MemoryPool`
- `feed_simulator.h` / `feed_simulator.cpp`: generates random test orders
- `main.cpp`: connects the simulator and the order book into a runnable demo
- `tests/test_matching.cpp`: test cases for insert/match/cancel (doctest)
- `tests/test_memory_pool.cpp`: test cases for the memory pool (allocation, reuse, growth, guard against `blockCount == 0`)

## Build & run

Requires `g++` (C++17) and `make`, for example via [MSYS2](https://www.msys2.org/) on Windows.

```
make          # builds app.exe and test.exe
./app.exe     # 10 simulated orders, book snapshot after each order
./test.exe    # test suite (doctest)
make clean    # clean up
```

## Example output

`app.exe` prints the best 5 price levels per side after every inserted order. If a match happens, the trade is printed live in between:

```
--- BID ---
102 |  [ 103 ]
--- ASK ---
103 |  [ 101 ]
105 |  [ 102 ]

Trade @ Quantity 102 @ Price: 102
--- BID ---
102 |  [ 1 ]
--- ASK ---
103 |  [ 101 ]
105 |  [ 102 ]
```

Each book line: `price | [remaining volume order 1] [remaining volume order 2] ...`, sorted by price time priority (best bid/ask first). The trade price is the price of the order that was already resting in the book (maker price); the newly arriving order may get price improvement. All trades are also stored in `trade_log` (for a later P&L calculation).

## Current status

The MVP (part 1 of [TODO.md](TODO.md)) is done. Steps 4 and 5 of the "receive side masterclass" are also complete: `std::list<Order>` is fully out of the project, replaced by a `MemoryPool` plus an intrusive linked list (`next_order`/`prev_order` live directly inside the `Order` struct; each price level in `bids`/`asks` is just an `OrderQueue{head, tail}`, no separate heap allocation per list node anymore).

Done:
- `MemoryPool` (allocation, reuse, growth, guard against `blockCount == 0`), tested independently
- `add_order`: pool allocation, placement new, linking to the end of the queue
- `cancel_order`: find the order via `order_map`, unlink it from the chain (4 cases: only element, head, tail, middle), destructor, `deallocate`
- `match()`: works on `head` and manual unlinking instead of `front()`/`pop_front()`
- `display()`, `order_volume()`: switched to pointer traversal

Builds cleanly with `make all`, no compiler warnings, all tests passing.

Next up, the rest of the "receive side masterclass":

- Benchmarking: naive `std::list` vs. pool version, p50/p99 latency
- A lock free SPSC queue to separate the producer (feed simulator) thread from the matching thread

## License

MIT, see [LICENSE](LICENSE).
