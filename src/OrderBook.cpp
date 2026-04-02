#include "OrderBook.h"
#include "FixConstants.h"
#include <sstream>
#include <ctime>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace oms {

int64_t OrderBook::s_orderCounter = 0;

OrderBook::OrderBook() {
    pthread_mutex_init(&d_mutex, NULL);
}

OrderBook::~OrderBook() {
    pthread_mutex_destroy(&d_mutex);
}

int64_t OrderBook::nowNs() {
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (int64_t)(cnt.QuadPart * 1000000000LL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

std::string OrderBook::generateOrderId() {
    // Atomic-style increment — called under mutex so safe
    ++s_orderCounter;
    std::ostringstream oss;
    oss << "ORD" << s_orderCounter;
    return oss.str();
}

OmsResult OrderBook::submitOrder(const fix::FixMessage& nos, Order& orderOut) {
    std::string clOrdId = nos.getField(fix::Tag::ClOrdID);
    if (clOrdId.empty())
        return OmsResult(OmsResult::InvalidState, "Missing ClOrdID (tag 11)");

    std::string symbol = nos.getField(fix::Tag::Symbol);
    if (symbol.empty())
        return OmsResult(OmsResult::InvalidState, "Missing Symbol (tag 55)");

    double qty = nos.getFieldAsDbl(fix::Tag::OrderQty);
    if (qty <= 0.0)
        return OmsResult(OmsResult::InvalidState, "Invalid OrderQty");

    pthread_mutex_lock(&d_mutex);

    // Duplicate check
    if (d_clOrdIdIndex.find(clOrdId) != d_clOrdIdIndex.end()) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::DuplicateOrder,
                         "Duplicate ClOrdID: " + clOrdId);
    }

    Order o;
    o.d_clOrdId     = clOrdId;
    o.d_orderId     = generateOrderId();
    o.d_symbol      = symbol;
    o.d_account     = nos.getField(fix::Tag::Account);
    o.d_side        = static_cast<fix::Side::Enum>(nos.getFieldAsChar(fix::Tag::Side));
    o.d_ordType     = static_cast<fix::OrdType::Enum>(nos.getFieldAsChar(fix::Tag::OrdType));
    o.d_tif         = static_cast<fix::TimeInForce::Enum>(nos.getFieldAsChar(fix::Tag::TimeInForce));
    o.d_orderQty    = qty;
    o.d_price       = nos.getFieldAsDbl(fix::Tag::Price);
    o.d_leavesQty   = qty;
    o.d_cumQty      = 0.0;
    o.d_avgPx       = 0.0;
    o.d_state       = OrderState::New;
    o.d_submitTimeNs = nowNs();

    d_orders[o.d_orderId]        = o;
    d_clOrdIdIndex[clOrdId]      = o.d_orderId;

    orderOut = o;
    pthread_mutex_unlock(&d_mutex);
    return OmsResult(OmsResult::Ok);
}

OmsResult OrderBook::cancelOrder(const std::string& clOrdId,
                                 const std::string& /*origClOrdId*/) {
    pthread_mutex_lock(&d_mutex);

    std::map<std::string,std::string>::iterator idx =
        d_clOrdIdIndex.find(clOrdId);
    if (idx == d_clOrdIdIndex.end()) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::NotFound, "Unknown ClOrdID: " + clOrdId);
    }

    Order& o = d_orders[idx->second];
    if (o.isTerminal()) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::InvalidState,
                         "Cannot cancel terminal order: " + clOrdId);
    }

    o.d_state     = OrderState::Canceled;
    o.d_leavesQty = 0.0;

    pthread_mutex_unlock(&d_mutex);
    return OmsResult(OmsResult::Ok);
}

OmsResult OrderBook::applyFill(const std::string& orderId,
                               double             fillQty,
                               double             fillPx) {
    pthread_mutex_lock(&d_mutex);

    std::map<std::string, Order>::iterator it = d_orders.find(orderId);
    if (it == d_orders.end()) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::NotFound, "Unknown orderId: " + orderId);
    }

    Order& o = it->second;
    if (o.isTerminal()) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::InvalidState, "Order is terminal");
    }
    if (fillQty <= 0.0 || fillQty > o.d_leavesQty) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::InvalidState, "Invalid fill quantity");
    }

    // Update average price (VWAP)
    double totalValue = o.d_avgPx * o.d_cumQty + fillPx * fillQty;
    o.d_cumQty   += fillQty;
    o.d_leavesQty = o.d_orderQty - o.d_cumQty;
    o.d_avgPx    = (o.d_cumQty > 0.0) ? (totalValue / o.d_cumQty) : 0.0;

    if (o.d_leavesQty <= 0.0)
        o.d_state = OrderState::Filled;
    else
        o.d_state = OrderState::PartiallyFilled;

    pthread_mutex_unlock(&d_mutex);
    return OmsResult(OmsResult::Ok);
}

OmsResult OrderBook::rejectOrder(const std::string& orderId,
                                 const std::string& /*reason*/) {
    pthread_mutex_lock(&d_mutex);

    std::map<std::string, Order>::iterator it = d_orders.find(orderId);
    if (it == d_orders.end()) {
        pthread_mutex_unlock(&d_mutex);
        return OmsResult(OmsResult::NotFound, "Unknown orderId: " + orderId);
    }

    it->second.d_state     = OrderState::Rejected;
    it->second.d_leavesQty = 0.0;

    pthread_mutex_unlock(&d_mutex);
    return OmsResult(OmsResult::Ok);
}

bool OrderBook::findByOrderId(const std::string& orderId, Order& out) const {
    pthread_mutex_lock(&d_mutex);
    std::map<std::string, Order>::const_iterator it = d_orders.find(orderId);
    bool found = (it != d_orders.end());
    if (found) out = it->second;
    pthread_mutex_unlock(&d_mutex);
    return found;
}

bool OrderBook::findByClOrdId(const std::string& clOrdId, Order& out) const {
    pthread_mutex_lock(&d_mutex);
    std::map<std::string,std::string>::const_iterator idx =
        d_clOrdIdIndex.find(clOrdId);
    bool found = false;
    if (idx != d_clOrdIdIndex.end()) {
        std::map<std::string, Order>::const_iterator it =
            d_orders.find(idx->second);
        if (it != d_orders.end()) {
            out   = it->second;
            found = true;
        }
    }
    pthread_mutex_unlock(&d_mutex);
    return found;
}

std::vector<Order> OrderBook::activeOrders() const {
    std::vector<Order> result;
    pthread_mutex_lock(&d_mutex);
    for (std::map<std::string, Order>::const_iterator it = d_orders.begin();
         it != d_orders.end(); ++it) {
        if (it->second.isActive())
            result.push_back(it->second);
    }
    pthread_mutex_unlock(&d_mutex);
    return result;
}

std::size_t OrderBook::totalOrderCount() const {
    pthread_mutex_lock(&d_mutex);
    std::size_t n = d_orders.size();
    pthread_mutex_unlock(&d_mutex);
    return n;
}

} // namespace oms
