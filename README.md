# C++ limit order book

A single-instrument C++20 simulator for a continuous limit order book. It accepts limit and market orders, applies price-time priority, emits deterministic trades and supports cancelling resting orders.

The point is one checkable rule: the best price trades first and, at the same price, the oldest resting order trades first. It is a simulator. It is not a broker connection or an exchange implementation.

**The benchmark is one local baseline, not an exchange-performance claim.** It measures single-threaded replay of synthetic events on one laptop. It does not measure order-entry latency, tail latency, concurrency, networking or risk controls.

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

## Benchmark

The Release benchmark replays one million deterministic synthetic events for each workload. It generates the workload once with seed 42, performs one warm-up replay, then reports the median of seven timed replays. Workload generation is outside the timed section.

| Workload | Median throughput |
|---|---:|
| Balanced | 3,358,413 events/s |
| Cancel-heavy | 5,448,125 events/s |
| Sweep | 4,747,578 events/s |

Measured on 30 September 2026 with MSVC 19.51 Release flags (`/O2 /Ob2 /DNDEBUG`) on an Intel Core i7-1165G7 laptop running Windows 11, 8 logical processors. The executable's checksum includes matching and book-state output, so the replayed work contributes to an observable result. See [benchmark details](benchmarks/RESULTS.md), including the exact command and limits.

## Files

- `include/order_book/types.hpp`: order requests, trades and depth views.
- `include/order_book/order_book.hpp`: matching, resting-book storage and cancellation lookup.
- `benchmarks/workload.hpp`: seeded event generator for a future benchmark.
- `benchmarks/order_book_benchmark.cpp`: timed replay of the seeded workloads.
- `benchmarks/RESULTS.md`: recorded environment, method and measured median results.
- `tests/`: the three test executables above.
- `docs/design.md`: the data-structure choices and the price-time rule.

## Not included

Multiple instruments, order amendments, stop or iceberg orders, auctions, self-trade prevention, fees, risk checks, persistence, networking and concurrent matching.

## Status

Matching engine, tests and a repeatable local benchmark are implemented. GitHub Actions builds the Release configuration and runs the three CTest targets. This was built with AI assistance (Codex), and I am treating it as a worked example to study and extend, not as proof I can write a matching engine unaided. My own from-memory C++ practice is kept in a separate repo.
