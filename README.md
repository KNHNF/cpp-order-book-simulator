# C++ limit order book

A single-instrument C++20 simulator for a continuous limit order book. It accepts limit and market orders, applies price-time priority, emits deterministic trades and supports cancellation of resting orders.

The project makes one matching rule checkable: the best available price trades first and, at the same price, the oldest resting order trades first. It is a simulator, not a broker connection or an exchange implementation.

## Built behaviour

- Limit buys and sells, including unfilled remainders resting on the book.
- Market buys and sells, with any unfilled market remainder discarded.
- Partial fills at the passive order's price.
- FIFO priority within each price level.
- Cancellation by active order ID.
- Read-only bid and ask depth views for test assertions.

The implementation uses ascending asks, descending bids, FIFO lists within each price level and an order-ID lookup for cancellation. This makes the price-time rule explicit. It is a readable first implementation, not a claim about exchange-grade latency or throughput.

## Test coverage

The executable test suite covers the 12 acceptance scenarios in the written specification: resting orders, passive-price execution, partial fills, market-order level walking, market remainders, price priority, FIFO, cancellation, price protection and invalid lifecycle actions. It also covers the symmetric sell-side path.

## Build and test

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

On 16 September 2026, the Debug build passed 1 CTest target containing 13 matching scenarios with MSVC 19.51 and CMake 4.3.1.

## Structure

- `include/order_book/types.hpp`: order requests, trades and depth views.
- `include/order_book/order_book.hpp`: matching, resting-book storage and cancellation lookup.
- `tests/order_book_contract_tests.cpp`: deterministic acceptance tests.
- `docs/design.md`: data-structure and price-time-priority contract.

## Not included

- Multiple instruments, amendments, stop or iceberg orders, auctions, self-trade prevention, fees, risk checks, persistence, networking or concurrent matching.
- Benchmarks. No latency or throughput figure is claimed until a benchmark method and measured results exist.

## Status

The matching engine and acceptance tests are implemented. C++ cold-practice evidence remains separate. A future benchmark must report the hardware, compiler flags, event mix, book depth and measured results.
