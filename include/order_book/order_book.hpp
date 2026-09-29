#pragma once

#include "order_book/types.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace order_book {

class OrderBook {
public:
    std::vector<Trade> submit(const OrderRequest& request) {
        validate(request);
        if (active_orders_.contains(request.id)) {
            throw std::invalid_argument("order ID is already active");
        }

        Quantity remaining = request.quantity;
        std::vector<Trade> trades;
        if (request.side == Side::Buy) {
            match_buy(request, remaining, trades);
        } else {
            match_sell(request, remaining, trades);
        }

        if (request.type == OrderType::Limit && remaining > 0) {
            rest(request, remaining);
        }
        return trades;
    }

    bool cancel(OrderId id) {
        const auto locator_it = active_orders_.find(id);
        if (locator_it == active_orders_.end()) {
            return false;
        }

        const Locator locator = locator_it->second;
        if (locator.side == Side::Buy) {
            auto level_it = bids_.find(locator.price_ticks);
            remove_from_level(level_it->second, locator.order_it, active_orders_);
            if (level_it->second.orders.empty()) {
                bids_.erase(level_it);
            }
        } else {
            auto level_it = asks_.find(locator.price_ticks);
            remove_from_level(level_it->second, locator.order_it, active_orders_);
            if (level_it->second.orders.empty()) {
                asks_.erase(level_it);
            }
        }
        return true;
    }

    std::vector<PriceLevelView> bids() const { return view(bids_); }
    std::vector<PriceLevelView> asks() const { return view(asks_); }

    std::optional<Quantity> remaining_quantity(OrderId id) const {
        const auto locator_it = active_orders_.find(id);
        if (locator_it == active_orders_.end()) {
            return std::nullopt;
        }
        return locator_it->second.order_it->remaining_quantity;
    }

    bool invariants_hold() const {
        std::unordered_set<OrderId> seen_order_ids;
        if (!validate_book(bids_, Side::Buy, seen_order_ids)) {
            return false;
        }
        if (!validate_book(asks_, Side::Sell, seen_order_ids)) {
            return false;
        }
        if (seen_order_ids.size() != active_orders_.size()) {
            return false;
        }
        if (!bids_.empty() && !asks_.empty() && bids_.begin()->first >= asks_.begin()->first) {
            return false;
        }
        return true;
    }

private:
    struct RestingOrder {
        OrderId id;
        Quantity remaining_quantity;
        std::uint64_t sequence_number;
    };

    struct PriceLevel {
        std::list<RestingOrder> orders;
        Quantity total_quantity = 0;
    };

    using OrderIterator = std::list<RestingOrder>::iterator;

    struct Locator {
        Side side;
        PriceTicks price_ticks;
        OrderIterator order_it;
    };

    using BidBook = std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>>;
    using AskBook = std::map<PriceTicks, PriceLevel>;

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<OrderId, Locator> active_orders_;
    std::uint64_t next_sequence_number_ = 1;

    static void validate(const OrderRequest& request) {
        if (request.id == 0) {
            throw std::invalid_argument("order ID must be positive");
        }
        if (request.quantity <= 0) {
            throw std::invalid_argument("order quantity must be positive");
        }
        if (request.type == OrderType::Limit && request.price_ticks <= 0) {
            throw std::invalid_argument("limit price must be positive");
        }
    }

    void match_buy(const OrderRequest& request, Quantity& remaining, std::vector<Trade>& trades) {
        while (remaining > 0 && !asks_.empty()) {
            auto level_it = asks_.begin();
            if (request.type == OrderType::Limit && level_it->first > request.price_ticks) {
                break;
            }
            execute_against_front(request, remaining, trades, level_it, asks_);
        }
    }

    void match_sell(const OrderRequest& request, Quantity& remaining, std::vector<Trade>& trades) {
        while (remaining > 0 && !bids_.empty()) {
            auto level_it = bids_.begin();
            if (request.type == OrderType::Limit && level_it->first < request.price_ticks) {
                break;
            }
            execute_against_front(request, remaining, trades, level_it, bids_);
        }
    }

    template <typename Book>
    void execute_against_front(const OrderRequest& request, Quantity& remaining, std::vector<Trade>& trades,
                               typename Book::iterator level_it, Book& book) {
        PriceLevel& level = level_it->second;
        RestingOrder& passive = level.orders.front();
        const Quantity executed = std::min(remaining, passive.remaining_quantity);
        trades.push_back(Trade{passive.id, request.id, level_it->first, executed});
        remaining -= executed;
        passive.remaining_quantity -= executed;
        level.total_quantity -= executed;
        if (passive.remaining_quantity == 0) {
            active_orders_.erase(passive.id);
            level.orders.pop_front();
        }
        if (level.orders.empty()) {
            book.erase(level_it);
        }
    }

    void rest(const OrderRequest& request, Quantity remaining) {
        if (request.side == Side::Buy) {
            auto [level_it, inserted] = bids_.try_emplace(request.price_ticks);
            add_to_level(level_it->second, request, remaining);
        } else {
            auto [level_it, inserted] = asks_.try_emplace(request.price_ticks);
            add_to_level(level_it->second, request, remaining);
        }
    }

    void add_to_level(PriceLevel& level, const OrderRequest& request, Quantity remaining) {
        level.orders.push_back(RestingOrder{request.id, remaining, next_sequence_number_++});
        auto order_it = std::prev(level.orders.end());
        level.total_quantity += remaining;
        active_orders_.emplace(request.id, Locator{request.side, request.price_ticks, order_it});
    }

    static void remove_from_level(PriceLevel& level, OrderIterator order_it,
                                  std::unordered_map<OrderId, Locator>& active_orders) {
        level.total_quantity -= order_it->remaining_quantity;
        active_orders.erase(order_it->id);
        level.orders.erase(order_it);
    }

    template <typename Book>
    bool validate_book(const Book& book, Side expected_side, std::unordered_set<OrderId>& seen_order_ids) const {
        for (const auto& [price_ticks, level] : book) {
            if (level.orders.empty() || level.total_quantity <= 0) {
                return false;
            }

            Quantity summed_quantity = 0;
            std::uint64_t previous_sequence_number = 0;
            for (const RestingOrder& order : level.orders) {
                if (order.id == 0 || order.remaining_quantity <= 0 ||
                    order.sequence_number <= previous_sequence_number || !seen_order_ids.insert(order.id).second) {
                    return false;
                }
                previous_sequence_number = order.sequence_number;
                summed_quantity += order.remaining_quantity;

                const auto locator_it = active_orders_.find(order.id);
                if (locator_it == active_orders_.end() || locator_it->second.side != expected_side ||
                    locator_it->second.price_ticks != price_ticks || locator_it->second.order_it->id != order.id) {
                    return false;
                }
            }
            if (summed_quantity != level.total_quantity) {
                return false;
            }
        }
        return true;
    }

    template <typename Book>
    static std::vector<PriceLevelView> view(const Book& book) {
        std::vector<PriceLevelView> levels;
        levels.reserve(book.size());
        for (const auto& [price_ticks, level] : book) {
            levels.push_back(PriceLevelView{price_ticks, level.total_quantity});
        }
        return levels;
    }
};

}
