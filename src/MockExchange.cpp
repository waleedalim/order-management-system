#include "MockExchange.h"
#include "FixConstants.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
static void usleep(int us) { Sleep(us / 1000 ? us / 1000 : 1); }
#else
#include <unistd.h>
#endif

namespace oms {

MockExchange::MockExchange(const Config& config)
    : d_config(config), d_execCounter(0) {
    pthread_mutex_init(&d_mutex, NULL);
    std::srand(static_cast<unsigned>(std::time(NULL)));
}

std::string MockExchange::nextExecId() {
    ++d_execCounter;
    std::ostringstream oss;
    oss << "EXEC" << std::setw(8) << std::setfill('0') << d_execCounter;
    return oss.str();
}

double MockExchange::simulateFillPrice(const Order& order) const {
    double base = (order.d_price > 0.0) ? order.d_price : 100.0;
    // Add small random slippage within configured range
    double slipPct = ((double)std::rand() / RAND_MAX) * d_config.d_priceSlippagePct;
    // Buy orders fill slightly above, sell slightly below (market impact)
    if (order.d_side == fix::Side::Buy)
        return base * (1.0 + slipPct / 100.0);
    else
        return base * (1.0 - slipPct / 100.0);
}

std::vector<FillEvent> MockExchange::processOrder(const Order& order) {
    std::vector<FillEvent> events;

    // Simulate network/matching latency
    int latencyUs = d_config.d_minLatencyUs +
        std::rand() % (d_config.d_maxLatencyUs - d_config.d_minLatencyUs + 1);
    usleep(latencyUs);

    // Determine if order gets filled
    double roll = ((double)std::rand() / RAND_MAX) * 100.0;
    if (roll > d_config.d_fillRatePct) {
        // Reject
        FillEvent ev;
        pthread_mutex_lock(&d_mutex);
        ev.d_execId  = nextExecId();
        pthread_mutex_unlock(&d_mutex);
        ev.d_orderId     = order.d_orderId;
        ev.d_fillQty     = 0.0;
        ev.d_fillPx      = 0.0;
        ev.d_isFinal     = true;
        ev.d_rejected    = true;
        ev.d_rejectReason = "No liquidity";
        events.push_back(ev);
        return events;
    }

    // Decide: full fill or partial fills (1–3 partials)
    double fillPx    = simulateFillPrice(order);
    double remaining = order.d_orderQty;

    int numPartials = 1;
    // 30% chance of partial fills (2–3 partials)
    if (((double)std::rand() / RAND_MAX) < 0.30)
        numPartials = 2 + std::rand() % 2;

    for (int i = 0; i < numPartials && remaining > 0.0; ++i) {
        FillEvent ev;
        pthread_mutex_lock(&d_mutex);
        ev.d_execId = nextExecId();
        pthread_mutex_unlock(&d_mutex);
        ev.d_orderId  = order.d_orderId;
        ev.d_rejected = false;

        if (i == numPartials - 1) {
            // Last partial: fill the rest
            ev.d_fillQty = remaining;
            ev.d_isFinal = true;
        } else {
            // Fill between 30% and 70% of remaining
            double pct   = 0.30 + ((double)std::rand() / RAND_MAX) * 0.40;
            ev.d_fillQty = std::max(1.0, (int)(remaining * pct) * 1.0);
            ev.d_isFinal = false;
        }

        ev.d_fillPx = fillPx;
        remaining  -= ev.d_fillQty;
        events.push_back(ev);

        // Brief pause between partials
        if (i < numPartials - 1) usleep(50);
    }

    return events;
}

fix::FixMessage MockExchange::buildExecutionReport(const Order&     order,
                                                   const FillEvent& fill,
                                                   int              seqNum) const {
    fix::FixMessage er(fix::MsgType::ExecutionReport);

    er.setField(fix::Tag::MsgSeqNum,  seqNum);
    er.setField(fix::Tag::SenderCompID, "MOCKEXCH");
    er.setField(fix::Tag::TargetCompID, "OMS");

    er.setField(fix::Tag::OrderID,   order.d_orderId);
    er.setField(fix::Tag::ClOrdID,   order.d_clOrdId);
    er.setField(fix::Tag::ExecID,    fill.d_execId);
    er.setField(fix::Tag::Symbol,    order.d_symbol);
    er.setField(fix::Tag::Side,      static_cast<char>(order.d_side));

    if (fill.d_rejected) {
        er.setField(fix::Tag::OrdStatus, static_cast<char>(fix::OrdStatus::Rejected));
        er.setField(fix::Tag::ExecType,  static_cast<char>(fix::ExecType::Rejected));
        er.setField(fix::Tag::Text,      fill.d_rejectReason);
        er.setField(fix::Tag::LeavesQty, 0.0);
        er.setField(fix::Tag::CumQty,    0.0);
        er.setField(fix::Tag::AvgPx,     0.0);
    } else {
        bool isFull = (order.d_cumQty + fill.d_fillQty >= order.d_orderQty);
        fix::OrdStatus::Enum status = isFull
            ? fix::OrdStatus::Filled
            : fix::OrdStatus::PartiallyFilled;
        fix::ExecType::Enum execType = isFull
            ? fix::ExecType::Fill
            : fix::ExecType::PartialFill;

        er.setField(fix::Tag::OrdStatus, static_cast<char>(status));
        er.setField(fix::Tag::ExecType,  static_cast<char>(execType));
        er.setField(31,    fill.d_fillPx, 4);
        er.setField(32,   fill.d_fillQty, 0);
        er.setField(fix::Tag::CumQty,    order.d_cumQty + fill.d_fillQty, 0);
        er.setField(fix::Tag::LeavesQty, order.d_leavesQty - fill.d_fillQty, 0);
        er.setField(fix::Tag::AvgPx,     fill.d_fillPx, 4);
    }

    return er;
}

} // namespace oms
