#include "OmsEngine.h"
#include "FixConstants.h"
#include "FixParser.h"
#include <iostream>
#include <sstream>
#include <cstring>

namespace oms {

OmsEngine::OmsEngine(const Config& config, OrderBus& bus)
    : d_config(config)
    , d_bus(bus)
    , d_riskEngine(config.d_riskConfig, d_positionMgr)
    , d_exchange(config.d_exchangeConfig)
    , d_running(false)
    , d_ordersProcessed(0)
    , d_fillsReceived(0)
    , d_rejectsIssued(0)
    , d_outSeqNum(1)
{
    std::memset(&d_thread, 0, sizeof(d_thread));
}

OmsEngine::~OmsEngine() {
    stop();
}

void OmsEngine::start() {
    d_running = true;
    pthread_create(&d_thread, NULL, &OmsEngine::threadFunc, this);
}

void OmsEngine::stop() {
    if (d_running) {
        d_running = false;
        pthread_join(d_thread, NULL);
    }
}

void* OmsEngine::threadFunc(void* arg) {
    static_cast<OmsEngine*>(arg)->processLoop();
    return NULL;
}

void OmsEngine::processLoop() {
    while (d_running) {
        fix::FixMessage msg;
        if (!d_bus.popInbound(msg)) {
            // Spin-wait (in production: use a condvar or timed wait)
            continue;
        }

        std::string msgType = msg.getMsgType();
        if (msgType == fix::MsgType::NewOrderSingle) {
            handleNewOrderSingle(msg);
        } else if (msgType == fix::MsgType::OrderCancelRequest) {
            handleCancelRequest(msg);
        }
    }
}

void OmsEngine::handleNewOrderSingle(const fix::FixMessage& msg) {
    // 1. Submit to order book (assigns orderId, transitions to New)
    Order order;
    OmsResult result = d_orderBook.submitOrder(msg, order);
    if (!result.ok()) {
        if (d_config.d_verbose)
            std::cerr << "[OMS] Submit failed: " << result.d_message << "\n";
        ++d_rejectsIssued;
        return;
    }

    // 2. Pre-trade risk check
    RiskCheckResult risk = d_riskEngine.checkOrder(order);
    if (!risk.d_passed) {
        d_orderBook.rejectOrder(order.d_orderId, risk.d_reason);
        if (d_config.d_verbose)
            std::cerr << "[RISK] Rejected " << order.d_clOrdId
                      << ": " << risk.d_reason << "\n";

        // Send reject ExecutionReport
        fix::FixMessage er(fix::MsgType::ExecutionReport);
        er.setField(fix::Tag::MsgSeqNum,   d_outSeqNum++);
        er.setField(fix::Tag::ClOrdID,     order.d_clOrdId);
        er.setField(fix::Tag::OrderID,     order.d_orderId);
        er.setField(fix::Tag::OrdStatus,   static_cast<char>(fix::OrdStatus::Rejected));
        er.setField(fix::Tag::ExecType,    static_cast<char>(fix::ExecType::Rejected));
        er.setField(fix::Tag::Symbol,      order.d_symbol);
        er.setField(fix::Tag::Side,        static_cast<char>(order.d_side));
        er.setField(fix::Tag::LeavesQty,   0.0);
        er.setField(fix::Tag::CumQty,      0.0);
        er.setField(fix::Tag::AvgPx,       0.0);
        er.setField(fix::Tag::Text,        risk.d_reason);
        publishExecutionReport(er);
        ++d_rejectsIssued;
        return;
    }

    ++d_ordersProcessed;
    if (d_config.d_verbose)
        std::cout << "[OMS] Order accepted: " << order.d_orderId
                  << " " << order.d_symbol
                  << " " << (order.d_side == fix::Side::Buy ? "BUY" : "SELL")
                  << " " << order.d_orderQty
                  << " @ " << order.d_price << "\n";

    // 3. Route to mock exchange and process fills
    std::vector<FillEvent> fills = d_exchange.processOrder(order);

    for (std::size_t i = 0; i < fills.size(); ++i) {
        const FillEvent& fill = fills[i];

        if (fill.d_rejected) {
            d_orderBook.rejectOrder(order.d_orderId, fill.d_rejectReason);
            ++d_rejectsIssued;
        } else {
            d_orderBook.applyFill(order.d_orderId, fill.d_fillQty, fill.d_fillPx);
            d_positionMgr.onFill(order.d_symbol, order.d_side,
                                 fill.d_fillQty, fill.d_fillPx);
            ++d_fillsReceived;
        }

        // Refresh order state for ER building
        Order updated;
        d_orderBook.findByOrderId(order.d_orderId, updated);

        fix::FixMessage er = d_exchange.buildExecutionReport(
            updated, fill, d_outSeqNum++);
        publishExecutionReport(er);

        if (d_config.d_verbose) {
            if (fill.d_rejected)
                std::cout << "[EXCH] Rejected: " << fill.d_rejectReason << "\n";
            else
                std::cout << "[EXCH] Fill " << fill.d_fillQty
                          << " @ " << fill.d_fillPx
                          << (fill.d_isFinal ? " (FINAL)" : " (PARTIAL)") << "\n";
        }
    }
}

void OmsEngine::handleCancelRequest(const fix::FixMessage& msg) {
    std::string clOrdId     = msg.getField(fix::Tag::ClOrdID);
    std::string origClOrdId = msg.getField(41);

    OmsResult r = d_orderBook.cancelOrder(clOrdId, origClOrdId);
    if (!r.ok()) {
        if (d_config.d_verbose)
            std::cerr << "[OMS] Cancel failed: " << r.d_message << "\n";
        return;
    }

    if (d_config.d_verbose)
        std::cout << "[OMS] Canceled: " << origClOrdId << "\n";

    // Build cancel ExecutionReport
    Order o;
    d_orderBook.findByClOrdId(clOrdId, o);

    fix::FixMessage er(fix::MsgType::ExecutionReport);
    er.setField(fix::Tag::MsgSeqNum,  d_outSeqNum++);
    er.setField(fix::Tag::ClOrdID,    clOrdId);
    er.setField(fix::Tag::OrderID,    o.d_orderId);
    er.setField(fix::Tag::ExecID,     "CANCEL");
    er.setField(fix::Tag::OrdStatus,  static_cast<char>(fix::OrdStatus::Canceled));
    er.setField(fix::Tag::ExecType,   static_cast<char>(fix::ExecType::Canceled));
    er.setField(fix::Tag::Symbol,     o.d_symbol);
    er.setField(fix::Tag::Side,       static_cast<char>(o.d_side));
    er.setField(fix::Tag::LeavesQty,  0.0);
    er.setField(fix::Tag::CumQty,     o.d_cumQty);
    er.setField(fix::Tag::AvgPx,      o.d_avgPx);
    publishExecutionReport(er);
}

void OmsEngine::publishExecutionReport(const fix::FixMessage& er) {
    // Best-effort push; if bus is full drop (log in production)
    if (!d_bus.pushOutbound(er)) {
        if (d_config.d_verbose)
            std::cerr << "[OMS] Outbound bus full, dropped ER\n";
    }
}

bool OmsEngine::submitDirect(const fix::FixMessage& nos) {
    return d_bus.pushInbound(nos);
}

std::size_t OmsEngine::ordersProcessed() const { return d_ordersProcessed; }
std::size_t OmsEngine::fillsReceived()   const { return d_fillsReceived;   }
std::size_t OmsEngine::rejectsIssued()   const { return d_rejectsIssued;   }

} // namespace oms
