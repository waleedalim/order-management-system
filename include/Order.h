#ifndef OMS_ORDER_H
#define OMS_ORDER_H

#include "FixConstants.h"
#include <string>
#include <stdint.h>

namespace oms {

// OrderState — finite state machine states for an order lifecycle.
// Transitions: PendingNew -> New -> Open -> PartiallyFilled -> Filled
//                                       -> PendingCancel   -> Canceled
//                                       -> Rejected
struct OrderState {
    enum Enum {
        PendingNew,
        New,
        Open,
        PartiallyFilled,
        Filled,
        PendingCancel,
        Canceled,
        Rejected
    };

    static const char* toString(Enum s);
    static fix::OrdStatus::Enum toFixOrdStatus(Enum s);
};

// Order — immutable identity fields set at creation, mutable state fields.
// BDE convention: d_ prefix for data members.
struct Order {
    // Identity (set once)
    std::string d_clOrdId;      // Client order ID (from FIX tag 11)
    std::string d_orderId;      // Internal OMS-assigned ID
    std::string d_symbol;       // e.g. "AAPL", "IBM"
    std::string d_account;      // Client account
    fix::Side::Enum     d_side;
    fix::OrdType::Enum  d_ordType;
    fix::TimeInForce::Enum d_tif;
    double      d_orderQty;
    double      d_price;        // 0.0 for market orders
    int64_t     d_submitTimeNs; // nanosecond timestamp

    // Mutable state
    OrderState::Enum d_state;
    double      d_cumQty;       // total filled quantity
    double      d_leavesQty;    // remaining quantity
    double      d_avgPx;        // average fill price
    int         d_seqNum;       // FIX sequence number

    Order();

    // Convenience
    bool isTerminal() const;    // Filled, Canceled, or Rejected
    bool isActive()   const;    // New, Open, or PartiallyFilled
};

} // namespace oms

#endif // OMS_ORDER_H
