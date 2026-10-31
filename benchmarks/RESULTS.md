# Benchmark results

Run date: 30 September 2026.

| Workload | Events | Measured repetitions | Median time | Median throughput |
|---|---:|---:|---:|---:|
| Balanced | 1,000,000 | 7 | 0.297760 s | 3,358,413 events/s |
| Cancel-heavy | 1,000,000 | 7 | 0.183549 s | 5,448,125 events/s |
| Sweep | 1,000,000 | 7 | 0.210634 s | 4,747,578 events/s |

Environment: Windows 11 build 26200, Intel Core i7-1165G7 at 2.80 GHz, 8 logical processors, MSVC 19.51.36257.0, CMake 4.3.1, Release build with `/O2 /Ob2 /DNDEBUG`.

Each workload is generated once with seed 42, then replayed once as warm-up and seven times for measurement. The reported value is the median of the seven timed replays. Timing starts after workload generation and includes `submit()` and `cancel()` calls only. The checksum includes trade quantities, successful cancellations and final depth counts so the matching work contributes to an observable result.

These are single-threaded, local measurements on one laptop. They do not measure order-entry latency, tail latency, memory use, concurrent access, network handling, risk controls or exchange throughput. They are a baseline for this readable simulator, not an exchange-performance claim.

Re-run from a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
.\build\Release\order_book_benchmark.exe
```

## Method note for reproducing or extending this benchmark

This note describes the method only. It contains no measurements. Anything you record should come from your own machine.

- Build in Release. Debug and sanitizer builds are not comparable.
- Record, next to any number: compiler and version, flags, CPU model, OS, power mode, and the date. Close other heavy programs and keep the laptop on mains power.
- The workload is generated once from a fixed seed and replayed from memory, so generation cost and random-number cost are excluded. The first replay is a warm-up and is discarded.
- Report the median of several replays, and keep the individual samples if you want to show spread. The executable prints only the median and a checksum, so add the samples to the output before quoting a range.
- The checksum must match between runs on the same compiler and seed. A different checksum means the matching behaviour changed, and the timings are then not comparable.
- Synthetic prices cluster within 5 ticks of a reference price, so books stay shallow and crossing is frequent. Results will differ for deeper or sparser books.
- Timings of `std::map` and `std::list` based books are dominated by allocation and cache behaviour, which vary a lot between machines. Do not compare numbers across machines.
- Not measured: per-order latency or its tail, memory use, multi-threading, I/O.
