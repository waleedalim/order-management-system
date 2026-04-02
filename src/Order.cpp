#include "Order.h"

namespace oms {

const char* OrderState::toString(Enum s) {
    switch (s) {
        case PendingNew:      return "PendingNew";
        case New:             return "New";
        case Open:            return "Open";
        case PartiallyFilled: return "PartiallyFilled";
        case Filled:          return "Filled";
        case PendingCancel:   return "PendingCancel";
        case Canceled:        return "Canceled";
        case Rejected:        return "Rejected";
        default:              return "Unknown";
    }
}

fix::OrdStatus::Enum OrderState::toFixOrdStatus(Enum s) {
    switch (s) {
        case PendingNew:      return fix::OrdStatus::PendingNew;
        case New:             return fix::OrdStatus::New;
        case Open:            return fix::OrdStatus::New;
        case PartiallyFilled: return fix::OrdStatus::PartiallyFilled;
        case Filled:          return fix::OrdStatus::Filled;
        case PendingCancel:   return fix::OrdStatus::PendingCancel;
        case Canceled:        return fix::OrdStatus::Canceled;
        case Rejected:        return fix::OrdStatus::Rejected;
        default:              return fix::OrdStatus::Rejected;
    }
}

Order::Order()
    : d_side(fix::Side::Buy)
    , d_ordType(fix::OrdType::Limit)
    , d_tif(fix::TimeInForce::Day)
    , d_orderQty(0.0)
    , d_price(0.0)
    , d_submitTimeNs(0)
    , d_state(OrderState::PendingNew)
    , d_cumQty(0.0)
    , d_leavesQty(0.0)
    , d_avgPx(0.0)
    , d_seqNum(0)
{}

bool Order::isTerminal() const {
    return d_state == OrderState::Filled   ||
           d_state == OrderState::Canceled ||
           d_state == OrderState::Rejected;
}

bool Order::isActive() const {
    return d_state == OrderState::New            ||
           d_state == OrderState::Open           ||
           d_state == OrderState::PartiallyFilled;
}

} // namespace oms
