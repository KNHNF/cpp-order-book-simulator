#pragma once

#include <cstdint>

namespace order_book {

using OrderId = std::uint64_t;
using PriceTicks = std::int64_t;
using Quantity = std::int64_t;

enum class Side { Buy, Sell };
enum class OrderType { Limit, Market };

struct OrderRequest {
    OrderId id;
    Side side;
    OrderType type;
    Quantity quantity;
    PriceTicks price_ticks = 0;
};

struct Trade {
    OrderId passive_order_id;
    OrderId aggressor_order_id;
    PriceTicks price_ticks;
    Quantity quantity;
};

struct PriceLevelView {
    PriceTicks price_ticks;
    Quantity quantity;
};

}
