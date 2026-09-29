#include "../benchmarks/workload.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

void same_seed_produces_same_events() {
    const order_book::benchmark::WorkloadConfig config{
        order_book::benchmark::WorkloadKind::Balanced, 500, 42};
    const auto first = order_book::benchmark::make_workload(config);
    const auto second = order_book::benchmark::make_workload(config);
    require(first.size() == second.size(), "same seed should keep event count");
    for (std::size_t index = 0; index < first.size(); ++index) {
        require(first[index].kind == second[index].kind, "same seed should keep event kind");
        require(first[index].request.id == second[index].request.id, "same seed should keep order ID");
        require(first[index].request.price_ticks == second[index].request.price_ticks, "same seed should keep price");
        require(first[index].cancel_id == second[index].cancel_id, "same seed should keep cancellation ID");
    }
}

void each_workload_has_requested_size() {
    for (const auto kind : {order_book::benchmark::WorkloadKind::Balanced,
                            order_book::benchmark::WorkloadKind::CancelHeavy,
                            order_book::benchmark::WorkloadKind::Sweep}) {
        const auto events = order_book::benchmark::make_workload({kind, 1'000, 7});
        require(events.size() == 1'000, "workload should honour requested event count");
    }
}

}

int main() {
    same_seed_produces_same_events();
    each_workload_has_requested_size();
    std::cout << "workload scenarios passed\n";
}
