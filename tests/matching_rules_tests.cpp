#include "order_book/order_book.hpp"

#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using order_book::OrderBook;
using order_book::OrderRequest;
using order_book::OrderType;
using order_book::PriceLevelView;
using order_book::Side;
using order_book::Trade;

namespace {

OrderRequest limit(order_book::OrderId id, Side side, order_book::PriceTicks price, order_book::Quantity quantity) {
    return OrderRequest{id, side, OrderType::Limit, quantity, price};
}

OrderRequest market(order_book::OrderId id, Side side, order_book::Quantity quantity) {
    return OrderRequest{id, side, OrderType::Market, quantity};
}

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

bool same_trade(const Trade& a, const Trade& b) {
    return a.passive_order_id == b.passive_order_id && a.aggressor_order_id == b.aggressor_order_id &&
           a.price_ticks == b.price_ticks && a.quantity == b.quantity;
}

void require_trades(const std::vector<Trade>& actual, std::initializer_list<Trade> expected) {
    require(actual.size() == expected.size(), "unexpected trade count");
    auto it = expected.begin();
    for (const Trade& trade : actual) {
        require(same_trade(trade, *it), "unexpected trade");
        ++it;
    }
}

void require_levels(const std::vector<PriceLevelView>& actual, std::initializer_list<PriceLevelView> expected) {
    require(actual.size() == expected.size(), "unexpected price-level count");
    auto it = expected.begin();
    for (const PriceLevelView& level : actual) {
        require(level.price_ticks == it->price_ticks && level.quantity == it->quantity, "unexpected price level");
        ++it;
    }
}

template <typename Fn>
std::string error_message(Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument& e) {
        return e.what();
    }
    return {};
}

void sell_side_fifo_at_one_bid() {
    OrderBook book;
    book.submit(limit(1, Side::Buy, 100, 3));
    book.submit(limit(2, Side::Buy, 100, 4));
    require_trades(book.submit(limit(3, Side::Sell, 100, 5)), {{1, 3, 100, 3}, {2, 3, 100, 2}});
    require_levels(book.bids(), {{100, 2}});
}

void price_beats_time_on_bid_side() {
    OrderBook book;
    book.submit(limit(1, Side::Buy, 100, 5));
    book.submit(limit(2, Side::Buy, 101, 5));
    require_trades(book.submit(limit(3, Side::Sell, 99, 6)), {{2, 3, 101, 5}, {1, 3, 100, 1}});
}

void incoming_order_does_not_jump_queue() {
    OrderBook book;
    book.submit(limit(1, Side::Buy, 100, 1));
    book.submit(limit(2, Side::Buy, 100, 1));
    book.submit(limit(3, Side::Buy, 100, 1));
    require_trades(book.submit(limit(4, Side::Sell, 100, 1)), {{1, 4, 100, 1}});
    require_trades(book.submit(limit(5, Side::Sell, 100, 1)), {{2, 5, 100, 1}});
}

void resubmitting_goes_to_back_of_queue() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 2));
    book.submit(limit(2, Side::Sell, 101, 2));
    require(book.cancel(1), "order 1 should cancel");
    book.submit(limit(3, Side::Sell, 101, 2));
    require_trades(book.submit(limit(4, Side::Buy, 101, 3)), {{2, 4, 101, 2}, {3, 4, 101, 1}});
}

void cancel_middle_of_queue() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 1));
    book.submit(limit(2, Side::Sell, 101, 2));
    book.submit(limit(3, Side::Sell, 101, 4));
    require(book.cancel(2), "middle order should cancel");
    require_levels(book.asks(), {{101, 5}});
    require_trades(book.submit(market(4, Side::Buy, 5)), {{1, 4, 101, 1}, {3, 4, 101, 4}});
    require_levels(book.asks(), {});
}

void cancel_last_order_removes_level() {
    OrderBook book;
    book.submit(limit(1, Side::Buy, 100, 1));
    book.submit(limit(2, Side::Buy, 99, 1));
    require(book.cancel(1), "best bid should cancel");
    require_levels(book.bids(), {{99, 1}});
}

void limit_boundary_is_inclusive_and_exclusive() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 1));
    require_trades(book.submit(limit(2, Side::Buy, 100, 1)), {});
    require_trades(book.submit(limit(3, Side::Buy, 101, 1)), {{1, 3, 101, 1}});
    book.submit(limit(4, Side::Buy, 100, 1));
    require_trades(book.submit(limit(5, Side::Sell, 101, 1)), {});
    require_trades(book.submit(limit(6, Side::Sell, 100, 2)), {{2, 6, 100, 1}, {4, 6, 100, 1}});
}

void sell_executes_at_passive_bid_price() {
    OrderBook book;
    book.submit(limit(1, Side::Buy, 100, 5));
    require_trades(book.submit(limit(2, Side::Sell, 95, 2)), {{1, 2, 100, 2}});
}

void market_order_on_empty_book() {
    OrderBook book;
    require_trades(book.submit(market(1, Side::Buy, 5)), {});
    require_trades(book.submit(market(2, Side::Sell, 5)), {});
    require_levels(book.bids(), {});
    require_levels(book.asks(), {});
    require(book.invariants_hold(), "empty book invariants");
}

void limit_remainder_rests_at_own_limit_after_sweep() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 100, 2));
    book.submit(limit(2, Side::Sell, 101, 2));
    book.submit(limit(3, Side::Sell, 103, 2));
    require_trades(book.submit(limit(4, Side::Buy, 102, 10)), {{1, 4, 100, 2}, {2, 4, 101, 2}});
    require_levels(book.bids(), {{102, 6}});
    require_levels(book.asks(), {{103, 2}});
    require(book.invariants_hold(), "book must not be crossed");
}

void ids_can_be_reused_once_inactive() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 1));
    book.submit(limit(2, Side::Buy, 101, 1));
    book.submit(limit(1, Side::Buy, 100, 1));
    require(book.cancel(1), "reused ID should cancel");
    book.submit(limit(1, Side::Buy, 100, 1));
    require(book.remaining_quantity(1) == 1, "ID reused after cancel");
}

void filled_order_cannot_be_cancelled() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 1));
    book.submit(limit(2, Side::Buy, 101, 1));
    require(!book.cancel(1), "filled order should not cancel");
    require(!book.cancel(2), "never-rested aggressor should not cancel");
}

void rejected_orders_leave_book_unchanged() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 3));
    const OrderRequest bad[] = {limit(0, Side::Buy, 100, 1),  limit(2, Side::Buy, 100, 0),
                                limit(3, Side::Buy, 100, -4), limit(4, Side::Buy, 0, 1),
                                limit(5, Side::Buy, -7, 1),   market(6, Side::Buy, 0),
                                market(7, Side::Sell, -1),    limit(1, Side::Buy, 105, 1)};
    for (const OrderRequest& request : bad) {
        require(!error_message([&] { book.submit(request); }).empty(), "invalid request should throw");
        require_levels(book.asks(), {{101, 3}});
        require_levels(book.bids(), {});
        require(book.invariants_hold(), "invariants after rejection");
    }
}

void error_messages_name_the_order() {
    OrderBook book;
    book.submit(limit(42, Side::Buy, 100, 1));
    require(error_message([&] { book.submit(limit(42, Side::Buy, 99, 1)); }).find("42") != std::string::npos,
            "duplicate error should name the ID");
    require(error_message([&] { book.submit(limit(7, Side::Buy, 100, -3)); }).find("-3") != std::string::npos,
            "quantity error should show the value");
    require(error_message([&] { book.submit(limit(8, Side::Buy, -9, 1)); }).find("-9") != std::string::npos,
            "price error should show the value");
}

void level_total_overflow_is_rejected() {
    OrderBook book;
    const auto big = std::numeric_limits<order_book::Quantity>::max();
    book.submit(limit(1, Side::Buy, 100, big));
    require(!error_message([&] { book.submit(limit(2, Side::Buy, 100, 1)); }).empty(), "overflow should throw");
    require_levels(book.bids(), {{100, big}});
    require(book.invariants_hold(), "invariants after overflow rejection");
    book.submit(limit(3, Side::Buy, 99, 1));
    require_levels(book.bids(), {{100, big}, {99, 1}});
}

// Independent reference: a flat list scanned for best price then lowest sequence.
class ReferenceBook {
public:
    std::vector<Trade> submit(const OrderRequest& r) {
        std::vector<Trade> trades;
        order_book::Quantity remaining = r.quantity;
        while (remaining > 0) {
            int best = -1;
            for (int i = 0; i < static_cast<int>(orders_.size()); ++i) {
                const Row& o = orders_[static_cast<std::size_t>(i)];
                if (o.side == r.side) continue;
                if (r.type == OrderType::Limit &&
                    (r.side == Side::Buy ? o.price > r.price_ticks : o.price < r.price_ticks)) {
                    continue;
                }
                if (best < 0) { best = i; continue; }
                const Row& b = orders_[static_cast<std::size_t>(best)];
                const bool better = (r.side == Side::Buy ? o.price < b.price : o.price > b.price) ||
                                    (o.price == b.price && o.seq < b.seq);
                if (better) best = i;
            }
            if (best < 0) break;
            Row& o = orders_[static_cast<std::size_t>(best)];
            const auto qty = std::min(remaining, o.qty);
            trades.push_back(Trade{o.id, r.id, o.price, qty});
            remaining -= qty;
            o.qty -= qty;
            if (o.qty == 0) orders_.erase(orders_.begin() + best);
        }
        if (r.type == OrderType::Limit && remaining > 0) {
            orders_.push_back(Row{r.id, r.side, r.price_ticks, remaining, next_seq_++});
        }
        return trades;
    }
    bool cancel(order_book::OrderId id) {
        for (std::size_t i = 0; i < orders_.size(); ++i) {
            if (orders_[i].id == id) { orders_.erase(orders_.begin() + static_cast<std::ptrdiff_t>(i)); return true; }
        }
        return false;
    }
    std::vector<PriceLevelView> depth(Side side) const {
        std::vector<PriceLevelView> levels;
        for (const Row& o : orders_) {
            if (o.side != side) continue;
            bool found = false;
            for (auto& l : levels) if (l.price_ticks == o.price) { l.quantity += o.qty; found = true; }
            if (!found) levels.push_back({o.price, o.qty});
        }
        for (std::size_t i = 0; i < levels.size(); ++i)
            for (std::size_t j = i + 1; j < levels.size(); ++j)
                if (side == Side::Buy ? levels[j].price_ticks > levels[i].price_ticks
                                      : levels[j].price_ticks < levels[i].price_ticks)
                    std::swap(levels[i], levels[j]);
        return levels;
    }
private:
    struct Row { order_book::OrderId id; Side side; order_book::PriceTicks price; order_book::Quantity qty; std::uint64_t seq; };
    std::vector<Row> orders_;
    std::uint64_t next_seq_ = 1;
};

void matches_reference_model(std::uint64_t seed) {
    OrderBook book;
    ReferenceBook reference;
    std::mt19937_64 gen(seed);
    std::vector<order_book::OrderId> ids;
    order_book::OrderId next_id = 1;
    for (int step = 0; step < 3'000; ++step) {
        if (gen() % 10 < 7 || ids.empty()) {
            const Side side = gen() % 2 == 0 ? Side::Buy : Side::Sell;
            const OrderType type = gen() % 6 == 0 ? OrderType::Market : OrderType::Limit;
            const auto qty = static_cast<order_book::Quantity>(gen() % 12 + 1);
            const auto price = static_cast<order_book::PriceTicks>(gen() % 7 + 97);
            const OrderRequest request{next_id++, side, type, qty, price};
            const auto actual = book.submit(request);
            const auto expected = reference.submit(request);
            require(actual.size() == expected.size(), "reference: trade count differs");
            for (std::size_t i = 0; i < actual.size(); ++i) require(same_trade(actual[i], expected[i]), "reference: trade differs");
            ids.push_back(request.id);
        } else {
            const auto id = ids[static_cast<std::size_t>(gen() % ids.size())];
            require(book.cancel(id) == reference.cancel(id), "reference: cancel result differs");
        }
        const auto bids = book.bids();
        const auto asks = book.asks();
        const auto rb = reference.depth(Side::Buy);
        const auto ra = reference.depth(Side::Sell);
        require(bids.size() == rb.size() && asks.size() == ra.size(), "reference: depth size differs");
        for (std::size_t i = 0; i < bids.size(); ++i)
            require(bids[i].price_ticks == rb[i].price_ticks && bids[i].quantity == rb[i].quantity, "reference: bid depth differs");
        for (std::size_t i = 0; i < asks.size(); ++i)
            require(asks[i].price_ticks == ra[i].price_ticks && asks[i].quantity == ra[i].quantity, "reference: ask depth differs");
    }
}

}

int main() {
    sell_side_fifo_at_one_bid();
    price_beats_time_on_bid_side();
    incoming_order_does_not_jump_queue();
    resubmitting_goes_to_back_of_queue();
    cancel_middle_of_queue();
    cancel_last_order_removes_level();
    limit_boundary_is_inclusive_and_exclusive();
    sell_executes_at_passive_bid_price();
    market_order_on_empty_book();
    limit_remainder_rests_at_own_limit_after_sweep();
    ids_can_be_reused_once_inactive();
    filled_order_cannot_be_cancelled();
    rejected_orders_leave_book_unchanged();
    error_messages_name_the_order();
    level_total_overflow_is_rejected();
    for (const std::uint64_t seed : {1ULL, 2ULL, 3ULL, 42ULL, 12345ULL}) matches_reference_model(seed);
    std::cout << "15 rule scenarios and 5 reference-model runs passed\n";
}
