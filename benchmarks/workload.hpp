#pragma once

#include "order_book/types.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace order_book::benchmark {

enum class WorkloadKind { Balanced, CancelHeavy, Sweep };
enum class EventKind { Submit, Cancel };

struct Event {
    EventKind kind;
    OrderRequest request{};
    OrderId cancel_id = 0;
};

struct WorkloadConfig {
    WorkloadKind kind;
    std::size_t event_count;
    std::uint64_t seed;
    PriceTicks reference_price = 10'000;
    Quantity maximum_quantity = 100;
};

inline std::vector<Event> make_workload(const WorkloadConfig& config) {
    if (config.event_count == 0 || config.reference_price <= 0 || config.maximum_quantity <= 0) {
        throw std::invalid_argument("workload configuration must be positive");
    }

    std::mt19937_64 generator(config.seed);
    std::vector<Event> events;
    std::vector<OrderId> active_limit_ids;
    events.reserve(config.event_count);
    active_limit_ids.reserve(config.event_count);
    OrderId next_order_id = 1;

    for (std::size_t event_index = 0; event_index < config.event_count; ++event_index) {
        const auto roll = generator() % 100;
        const bool cancel = !active_limit_ids.empty() &&
                            ((config.kind == WorkloadKind::CancelHeavy && roll < 45) ||
                             (config.kind == WorkloadKind::Balanced && roll < 15));
        if (cancel) {
            const auto index = static_cast<std::size_t>(generator() % active_limit_ids.size());
            events.push_back(Event{EventKind::Cancel, {}, active_limit_ids[index]});
            active_limit_ids.erase(active_limit_ids.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        const Side side = (generator() % 2 == 0) ? Side::Buy : Side::Sell;
        const bool sweep = config.kind == WorkloadKind::Sweep && roll < 30;
        const OrderType type = sweep ? OrderType::Market : OrderType::Limit;
        const Quantity quantity = static_cast<Quantity>((generator() % config.maximum_quantity) + 1);
        const PriceTicks offset = static_cast<PriceTicks>(generator() % 11) - 5;
        const OrderId id = next_order_id++;
        events.push_back(Event{EventKind::Submit, OrderRequest{id, side, type, quantity, config.reference_price + offset}});
        if (type == OrderType::Limit) {
            active_limit_ids.push_back(id);
        }
    }
    return events;
}

}
