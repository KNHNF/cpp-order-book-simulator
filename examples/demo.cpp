#include "order_book/order_book.hpp"

#include <iostream>

using namespace order_book;

namespace {

void print_trades(const char* label, const std::vector<Trade>& trades) {
    std::cout << label << ": " << trades.size() << " trade(s)\n";
    for (const Trade& t : trades) {
        std::cout << "  passive=" << t.passive_order_id << " aggressor=" << t.aggressor_order_id
                  << " price_ticks=" << t.price_ticks << " quantity=" << t.quantity << '\n';
    }
}

void print_book(const OrderBook& book) {
    std::cout << "asks (best first):";
    for (const auto& l : book.asks()) std::cout << " " << l.quantity << "@" << l.price_ticks;
    std::cout << "\nbids (best first):";
    for (const auto& l : book.bids()) std::cout << " " << l.quantity << "@" << l.price_ticks;
    std::cout << "\n\n";
}

}

int main() {
    OrderBook book;
    // Prices are integer ticks. The numbers are illustrative, not market data.
    book.submit({1, Side::Sell, OrderType::Limit, 5, 101});
    book.submit({2, Side::Sell, OrderType::Limit, 5, 101});
    book.submit({3, Side::Sell, OrderType::Limit, 5, 102});
    std::cout << "after three resting sells\n";
    print_book(book);

    // Buys 7 at up to 101: order 1 (oldest) fills first, then order 2 partly.
    print_trades("limit buy 7 @ 101", book.submit({4, Side::Buy, OrderType::Limit, 7, 101}));
    print_book(book);

    // Market buy sweeps the rest of 101, then 102, and discards any remainder.
    print_trades("market buy 20", book.submit({5, Side::Buy, OrderType::Market, 20}));
    print_book(book);
}
