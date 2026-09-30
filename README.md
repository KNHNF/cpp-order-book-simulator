# C++ limit order book

A single-instrument C++20 simulator for a continuous limit order book. It accepts limit and market orders, applies price-time priority, emits deterministic trades and supports cancelling resting orders.

The point is one checkable rule: the best price trades first and, at the same price, the oldest resting order trades first. It is a simulator. It is not a broker connection or an exchange implementation.

**There are no speed figures.** The matching code is a readable first version, and nothing here claims exchange-grade latency or throughput. A workload generator exists for a future benchmark, but no benchmark has been run.

## What it does

- Limit buys and sells, with any unfilled remainder resting on the book.
- Market buys and sells, with any unfilled remainder discarded.
- Partial fills at the resting order's price.
- First in, first out within each price level.
- Cancellation by order ID.
- Read-only bid and ask depth views, used by the tests.

Asks are kept ascending, bids descending, each price level is a FIFO list, and a lookup by order ID makes cancellation direct.

## Tests

Three test executables, each registered with CTest:

- `order_book_contract_tests`: the written acceptance scenarios (resting orders, passive-price execution, partial fills, walking price levels with a market order, price priority, FIFO, cancellation, invalid actions) plus the sell-side mirror.
- `order_book_invariant_tests`: randomised sequences checked against book invariants.
- `workload_contract_tests`: checks the seeded workload generator (balanced, cancel-heavy and sweep mixes) gives the same events for the same seed.

## Build and test

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

On 29 September 2026 the Debug build passed all three test executables (contract, invariant and workload), 3 of 3 in CTest, from a Visual Studio 2026 Developer PowerShell (version 18.10). On 16 September the contract tests passed with MSVC 19.51 and CMake 4.3.1, before the other two were added.

## Files

- `include/order_book/types.hpp`: order requests, trades and depth views.
- `include/order_book/order_book.hpp`: matching, resting-book storage and cancellation lookup.
- `benchmarks/workload.hpp`: seeded event generator for a future benchmark.
- `tests/`: the three test executables above.
- `docs/design.md`: the data-structure choices and the price-time rule.

## Not included

Multiple instruments, order amendments, stop or iceberg orders, auctions, self-trade prevention, fees, risk checks, persistence, networking and concurrent matching.

## Next

A benchmark that reports hardware, compiler flags, event mix, book depth and the measured numbers. Until then no performance claim is made.

## Status

Matching engine and tests are implemented. This was built with AI assistance (Codex), and I am treating it as a worked example to study and extend, not as proof I can write a matching engine unaided. My own from-memory C++ practice is kept in a separate repo.
