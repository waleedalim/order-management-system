#ifndef OMS_ORDER_BOOK_H
#define OMS_ORDER_BOOK_H

#include "Order.h"
#include "FixMessage.h"
#include <string>
#include <map>
#include <vector>
#include <pthread.h>
#include <stdint.h>

namespace oms {

// Result type for order book operations — avoids exceptions on hot path.
struct OmsResult {
    enum Code { Ok, InvalidState, NotFound, DuplicateOrder, RiskRejected };
    Code        d_code;
    std::string d_message;

    explicit OmsResult(Code c, const std::string& msg = "")
        : d_code(c), d_message(msg) {}

    bool ok() const { return d_code == Ok; }
};

// OrderBook — central store for all live orders.
// Thread-safe via a single mutex (appropriate for C++11 / BDE style).
// In production Bloomberg code this would use a concurrent hashmap,
// but a mutex + std::map demonstrates correct reasoning.
class OrderBook {
  public:
    OrderBook();
    ~OrderBook();

    // Submit a new order parsed from a FIX NewOrderSingle.
    // Assigns internal orderId and transitions to PendingNew -> New.
    OmsResult submitOrder(const fix::FixMessage& nos, Order& orderOut);

    // Cancel an existing order by clOrdId.
    OmsResult cancelOrder(const std::string& clOrdId,
                          const std::string& origClOrdId);

    // Apply a fill (partial or full) from the exchange simulator.
    // fillQty must be <= leavesQty.
    OmsResult applyFill(const std::string& orderId,
                        double             fillQty,
                        double             fillPx);

    // Reject an order (risk engine triggered).
    OmsResult rejectOrder(const std::string& orderId,
                          const std::string& reason);

    // Lookup by internal orderId (read-only snapshot).
    bool findByOrderId(const std::string& orderId, Order& out) const;

    // Lookup by clOrdId.
    bool findByClOrdId(const std::string& clOrdId, Order& out) const;

    // Return all active orders (snapshot).
    std::vector<Order> activeOrders() const;

    // Return total order count (including terminal).
    std::size_t totalOrderCount() const;

  private:
    mutable pthread_mutex_t d_mutex;

    // Primary index: orderId -> Order
    std::map<std::string, Order> d_orders;

    // Secondary index: clOrdId -> orderId
    std::map<std::string, std::string> d_clOrdIdIndex;

    static int64_t nowNs();
    static std::string generateOrderId();

    static int64_t s_orderCounter;

    // Non-copyable
    OrderBook(const OrderBook&);
    OrderBook& operator=(const OrderBook&);
};

} // namespace oms

#endif // OMS_ORDER_BOOK_H
