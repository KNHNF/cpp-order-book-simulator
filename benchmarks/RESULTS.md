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
