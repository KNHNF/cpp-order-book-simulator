# Design contract

Prices and quantities are integer values. Bids are stored in descending price order. Asks are stored in ascending price order. Within a price level, the oldest resting order has priority.

The implementation uses an ordered map for each side, a FIFO `std::list` at each price and an `std::unordered_map` from active order ID to its list position. The lookup permits cancellation without scanning every order. Aggregate quantity is stored at each price level and is exposed through read-only depth views for tests.

An incoming order is the aggressor. A resting order is passive. Each trade executes at the passive order's price. A buy limit order matches while the best ask is less than or equal to its limit. A sell limit order matches while the best bid is greater than or equal to its limit. A market order continues while opposing liquidity exists, then discards any remaining quantity.

The implementation is deliberately single-threaded and single-instrument. It is a simulator. It does not claim exchange-grade performance.
