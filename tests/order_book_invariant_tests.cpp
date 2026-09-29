#include "order_book/order_book.hpp"

#include <cstdlib>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

using order_book::OrderBook;
using order_book::OrderRequest;
using order_book::OrderType;
using order_book::Side;

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

void require_invariants(const OrderBook& book) {
    require(book.invariants_hold(), "order-book invariants must hold");
}

void lifecycle_actions_preserve_invariants() {
    OrderBook book;
    require_invariants(book);
    book.submit(OrderRequest{1, Side::Buy, OrderType::Limit, 5, 100});
    require_invariants(book);
    book.submit(OrderRequest{2, Side::Sell, OrderType::Limit, 4, 101});
    require_invariants(book);
    book.submit(OrderRequest{3, Side::Sell, OrderType::Limit, 3, 100});
    require_invariants(book);
    require(book.cancel(1), "resting order should cancel");
    require_invariants(book);
    book.submit(OrderRequest{4, Side::Buy, OrderType::Market, 10});
    require_invariants(book);
}

void seeded_event_stream_preserves_invariants(std::uint64_t seed) {
    OrderBook book;
    std::mt19937_64 generator(seed);
    std::vector<order_book::OrderId> submitted_limit_ids;
    order_book::OrderId next_order_id = 1;

    for (int event_index = 0; event_index < 2'000; ++event_index) {
        const auto action = generator() % 10;
        if (action < 7) {
            const Side side = (generator() % 2 == 0) ? Side::Buy : Side::Sell;
            const auto type = (generator() % 5 == 0) ? OrderType::Market : OrderType::Limit;
            const auto quantity = static_cast<order_book::Quantity>((generator() % 20) + 1);
            const auto price = static_cast<order_book::PriceTicks>((generator() % 21) + 90);
            const auto id = next_order_id++;
            book.submit(OrderRequest{id, side, type, quantity, price});
            if (type == OrderType::Limit && book.remaining_quantity(id).has_value()) {
                submitted_limit_ids.push_back(id);
            }
        } else if (!submitted_limit_ids.empty()) {
            const auto index = static_cast<std::size_t>(generator() % submitted_limit_ids.size());
            const auto id = submitted_limit_ids[index];
            book.cancel(id);
            submitted_limit_ids.erase(submitted_limit_ids.begin() + static_cast<std::ptrdiff_t>(index));
        }
        require_invariants(book);
    }
}

}

int main() {
    lifecycle_actions_preserve_invariants();
    seeded_event_stream_preserves_invariants(7);
    seeded_event_stream_preserves_invariants(42);
    seeded_event_stream_preserves_invariants(9'001);
    std::cout << "invariant scenarios passed\n";
}
