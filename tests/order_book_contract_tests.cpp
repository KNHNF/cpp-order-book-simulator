#include "order_book/order_book.hpp"

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
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

void require_trades(const std::vector<Trade>& actual, std::initializer_list<Trade> expected) {
    require(actual.size() == expected.size(), "unexpected trade count");
    auto expected_it = expected.begin();
    for (const Trade& trade : actual) {
        require(trade.passive_order_id == expected_it->passive_order_id, "unexpected passive order ID");
        require(trade.aggressor_order_id == expected_it->aggressor_order_id, "unexpected aggressor order ID");
        require(trade.price_ticks == expected_it->price_ticks, "unexpected execution price");
        require(trade.quantity == expected_it->quantity, "unexpected execution quantity");
        ++expected_it;
    }
}

void require_levels(const std::vector<PriceLevelView>& actual, std::initializer_list<PriceLevelView> expected) {
    require(actual.size() == expected.size(), "unexpected price-level count");
    auto expected_it = expected.begin();
    for (const PriceLevelView& level : actual) {
        require(level.price_ticks == expected_it->price_ticks, "unexpected price level");
        require(level.quantity == expected_it->quantity, "unexpected price-level quantity");
        ++expected_it;
    }
}

void non_marketable_limit_rests() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 10));
    require_trades(book.submit(limit(2, Side::Buy, 100, 10)), {});
    require_levels(book.bids(), {{100, 10}});
    require_levels(book.asks(), {{101, 10}});
}

void passive_price_execution() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 10));
    require_trades(book.submit(limit(2, Side::Buy, 105, 4)), {{1, 2, 101, 4}});
    require_levels(book.asks(), {{101, 6}});
}

void incoming_partial_fill_rests() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 3));
    require_trades(book.submit(limit(2, Side::Buy, 105, 8)), {{1, 2, 101, 3}});
    require_levels(book.bids(), {{105, 5}});
    require_levels(book.asks(), {});
}

void market_order_walks_levels() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 100, 2));
    book.submit(limit(2, Side::Sell, 101, 3));
    book.submit(limit(3, Side::Sell, 102, 4));
    require_trades(book.submit(market(4, Side::Buy, 7)), {{1, 4, 100, 2}, {2, 4, 101, 3}, {3, 4, 102, 2}});
    require_levels(book.asks(), {{102, 2}});
}

void market_remainder_cancels() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 100, 3));
    require_trades(book.submit(market(2, Side::Buy, 5)), {{1, 2, 100, 3}});
    require_levels(book.bids(), {});
    require_levels(book.asks(), {});
    require(!book.remaining_quantity(2).has_value(), "market remainder must not rest");
}

void better_price_has_priority() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 102, 5));
    book.submit(limit(2, Side::Sell, 101, 5));
    require_trades(book.submit(limit(3, Side::Buy, 105, 6)), {{2, 3, 101, 5}, {1, 3, 102, 1}});
    require_levels(book.asks(), {{102, 4}});
}

void fifo_at_one_price() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 3));
    book.submit(limit(2, Side::Sell, 101, 4));
    require_trades(book.submit(limit(3, Side::Buy, 101, 5)), {{1, 3, 101, 3}, {2, 3, 101, 2}});
    require(book.remaining_quantity(2) == 2, "later FIFO order should retain its remainder");
}

void partial_fill_keeps_priority() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 5));
    book.submit(limit(2, Side::Sell, 101, 5));
    require_trades(book.submit(limit(3, Side::Buy, 101, 2)), {{1, 3, 101, 2}});
    require_trades(book.submit(limit(4, Side::Buy, 101, 4)), {{1, 4, 101, 3}, {2, 4, 101, 1}});
}

void cancel_removes_remainder() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 5));
    require_trades(book.submit(limit(2, Side::Buy, 101, 2)), {{1, 2, 101, 2}});
    require(book.cancel(1), "resting order should cancel");
    require_trades(book.submit(limit(3, Side::Buy, 105, 5)), {});
    require_levels(book.bids(), {{105, 5}});
    require_levels(book.asks(), {});
}

void cancel_preserves_later_fifo() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 3));
    book.submit(limit(2, Side::Sell, 101, 4));
    require(book.cancel(1), "first order should cancel");
    require_trades(book.submit(limit(3, Side::Buy, 101, 4)), {{2, 3, 101, 4}});
}

void limit_price_protection() {
    OrderBook book;
    book.submit(limit(1, Side::Sell, 101, 2));
    book.submit(limit(2, Side::Sell, 103, 3));
    require_trades(book.submit(limit(3, Side::Buy, 101, 5)), {{1, 3, 101, 2}});
    require_levels(book.bids(), {{101, 3}});
    require_levels(book.asks(), {{103, 3}});
}

void sell_side_matches_best_bid_first() {
    OrderBook book;
    book.submit(limit(1, Side::Buy, 102, 2));
    book.submit(limit(2, Side::Buy, 101, 3));
    require_trades(book.submit(market(3, Side::Sell, 4)), {{1, 3, 102, 2}, {2, 3, 101, 2}});
    require_levels(book.bids(), {{101, 1}});
}

void invalid_lifecycle_rejected() {
    OrderBook book;
    require(!book.cancel(99), "unknown cancellation should fail");
    try {
        book.submit(limit(1, Side::Buy, 100, 0));
        fail("zero quantity should throw");
    } catch (const std::invalid_argument&) {
    }
    book.submit(limit(1, Side::Buy, 100, 2));
    try {
        book.submit(limit(1, Side::Buy, 99, 2));
        fail("duplicate active ID should throw");
    } catch (const std::invalid_argument&) {
    }
    require(book.cancel(1), "active order should cancel");
    require(!book.cancel(1), "cancelled order should not cancel twice");
    require_levels(book.bids(), {});
}

}

int main() {
    non_marketable_limit_rests();
    passive_price_execution();
    incoming_partial_fill_rests();
    market_order_walks_levels();
    market_remainder_cancels();
    better_price_has_priority();
    fifo_at_one_price();
    partial_fill_keeps_priority();
    cancel_removes_remainder();
    cancel_preserves_later_fifo();
    limit_price_protection();
    sell_side_matches_best_bid_first();
    invalid_lifecycle_rejected();
    std::cout << "13 matching-engine scenarios passed\n";
}
