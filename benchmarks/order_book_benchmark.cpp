#include "workload.hpp"
#include "order_book/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Measurement {
    double elapsed_seconds;
    std::uint64_t checksum;
};

Measurement replay(const std::vector<order_book::benchmark::Event>& events) {
    order_book::OrderBook book;
    std::uint64_t checksum = 0;

    const auto start = Clock::now();
    for (const auto& event : events) {
        if (event.kind == order_book::benchmark::EventKind::Submit) {
            const auto trades = book.submit(event.request);
            checksum += trades.size();
            for (const auto& trade : trades) {
                checksum += static_cast<std::uint64_t>(trade.quantity);
            }
        } else {
            checksum += book.cancel(event.cancel_id) ? 1U : 0U;
        }
    }
    const auto end = Clock::now();

    checksum += book.bids().size() + book.asks().size();
    return {std::chrono::duration<double>(end - start).count(), checksum};
}

const char* workload_name(order_book::benchmark::WorkloadKind kind) {
    switch (kind) {
    case order_book::benchmark::WorkloadKind::Balanced:
        return "balanced";
    case order_book::benchmark::WorkloadKind::CancelHeavy:
        return "cancel-heavy";
    case order_book::benchmark::WorkloadKind::Sweep:
        return "sweep";
    }
    return "unknown";
}

void benchmark(order_book::benchmark::WorkloadKind kind) {
    constexpr std::size_t event_count = 1'000'000;
    constexpr std::size_t repetitions = 7;
    const auto events = order_book::benchmark::make_workload({kind, event_count, 42});

    const Measurement warmup = replay(events);
    std::vector<double> samples;
    samples.reserve(repetitions);
    std::uint64_t checksum = warmup.checksum;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        const Measurement measurement = replay(events);
        samples.push_back(measurement.elapsed_seconds);
        checksum += measurement.checksum;
    }
    std::sort(samples.begin(), samples.end());
    const double median_seconds = samples[samples.size() / 2];
    const double events_per_second = static_cast<double>(event_count) / median_seconds;

    std::cout << workload_name(kind) << ',' << event_count << ',' << repetitions << ','
              << std::fixed << std::setprecision(6) << median_seconds << ','
              << std::setprecision(0) << events_per_second << ',' << checksum << '\n';
}

}

int main() {
    std::cout << "workload,events,repetitions,median_seconds,events_per_second,checksum\n";
    for (const auto kind : {order_book::benchmark::WorkloadKind::Balanced,
                            order_book::benchmark::WorkloadKind::CancelHeavy,
                            order_book::benchmark::WorkloadKind::Sweep}) {
        benchmark(kind);
    }
}
