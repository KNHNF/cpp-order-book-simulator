# Review result, 2 November 2026 session

Branch: `cloud/cpp-order-book-simulator-2026-11-02`. Nothing merged to main. No pull request opened.

## What I read

README, CMakeLists.txt, CI workflow, `include/order_book/*.hpp`, all three existing test files, `benchmarks/workload.hpp`, `benchmarks/order_book_benchmark.cpp`, `benchmarks/RESULTS.md`, `docs/design.md`. I did not read git history beyond the log.

## Findings

Matching logic: no price-time priority bug found. Best price first, FIFO within a level, trades at the passive price, market remainder discarded, partial fills keep queue position. All confirmed by the new tests and by the reference-model comparison.

Defects fixed, each with a test:

1. Signed integer overflow (undefined behaviour). Two resting orders of `INT64_MAX` at one price overflowed the level total. Reproduced under UBSan before the fix. Now rejected with `std::invalid_argument` before any state changes. Test: `level_total_overflow_is_rejected`. The check is conservative: it uses the full order quantity, not the post-match remainder, so it can reject slightly early, but only within `quantity` of `INT64_MAX`.
2. Unclear errors. Exceptions did not say which order or what value. They now include the order ID and the offending value. Test: `error_messages_name_the_order`.

Gaps filled:

- No Linux build instructions, no runnable example, stale "future benchmark" wording, no limits section, CI on Windows only. README, `examples/demo.cpp`, and a Windows plus Ubuntu CI matrix address these.
- Benchmark method note added to `benchmarks/RESULTS.md`. It contains no numbers.

## Tests, real output

Environment: g++ 13.3.0, CMake 3.28.3, Linux, Debug with `-Wall -Wextra -fsanitize=address,undefined`. No compiler warnings.

Before my changes (3 targets):

```
1/3 Test #1: order_book_contract_tests ........   Passed    0.03 sec
2/3 Test #2: order_book_invariant_tests .......   Passed    0.22 sec
3/3 Test #3: workload_contract_tests ..........   Passed    0.02 sec
100% tests passed, 0 tests failed out of 3
```

After my changes (4 targets):

```
1/4 Test #1: order_book_contract_tests ........   Passed    0.01 sec
2/4 Test #2: order_book_invariant_tests .......   Passed    0.21 sec
3/4 Test #3: workload_contract_tests ..........   Passed    0.02 sec
4/4 Test #4: matching_rules_tests .............   Passed    0.49 sec
100% tests passed, 0 tests failed out of 4
```

A Release build with g++ also passed 4 of 4. `matching_rules_tests` prints: `15 rule scenarios and 5 reference-model runs passed`.

Checking the tests can fail: a LIFO mutant (back instead of front of the level) made `matching_rules_tests` fail with `unexpected trade`. A mutant changing the limit boundary from `>` to `>=` failed with `unexpected trade count`. Mutants were built in the scratchpad, not committed.

Example output (`order_book_demo`), abridged: the limit buy of 7 at 101 produced trades against order 1 (5) then order 2 (2), in that order.

## Could not check

- MSVC, Visual Studio, Windows, macOS and clang builds. Only g++ 13 on Linux.
- The new CI workflow. I cannot run GitHub Actions here. The Ubuntu job is untested.
- The benchmark. I did not run it for the README, and I did not add any numbers.

## Unverified claims already in the repo (not mine)

- README and `benchmarks/RESULTS.md` throughput figures, the i7-1165G7 / Windows 11 / MSVC 19.51 environment, and the 29 and 30 September run dates. I cannot confirm any of them from here.
- "Visual Studio 2026 (version 18.10)" and "CMake 4.3.1".
- README states the code was built with AI assistance (Codex). I left it as written.

## Still missing before it reads as portfolio-ready

- A verified Linux and Windows CI run on the pushed branch.
- Benchmark samples printed individually (min, median, max), plus a rerun on current hardware with the method note followed.
- A latency profile or at least a comment on allocation cost: `std::list` and `std::map` per order are the obvious bottleneck, and an alternative (flat vector per level, pooled nodes) with a before and after would show engineering judgement.
- Order amendment (modify) with the rule for priority loss, and a decision on self-trade prevention, if you want to talk about exchange rules.
- An explanation you can give unaided of why `std::list` iterators stay valid, and why the cancel path is O(log n) in price levels, because the README says the code was AI-assisted and interviewers will probe it.
- Tick-size and lot-size validation if you want to claim realistic order checks.
- Unit-test framework is hand-rolled `require`. Fine for this size, but a failure stops at the first assertion without naming the line.

## Needs the laptop

- Run the Windows Debug and Release builds and tests.
- Re-run the benchmark and confirm or update the numbers.
- Check the Actions run on this branch before merging.
- Decide whether to merge, and whether the Ubuntu CI job stays.
