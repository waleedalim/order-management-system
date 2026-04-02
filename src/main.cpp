#include <iomanip>
// main.cpp — OMS end-to-end demo
// Simulates a session: connect gateway, submit orders, receive fills.

#include "OmsEngine.h"
#include "OrderBus.h"
#include "FixMessage.h"
#include "FixConstants.h"

#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
static void msleep(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void msleep(int ms) { usleep(ms * 1000); }
#endif

static int g_seqNum = 1;

static oms::fix::FixMessage makeNOS(const std::string& clOrdId,
                                    const std::string& symbol,
                                    char               side,
                                    double             qty,
                                    double             price,
                                    char               ordType = oms::fix::OrdType::Limit) {
    oms::fix::FixMessage nos(oms::fix::MsgType::NewOrderSingle);
    nos.setField(oms::fix::Tag::MsgSeqNum,    g_seqNum++);
    nos.setField(oms::fix::Tag::SenderCompID, "CLIENT1");
    nos.setField(oms::fix::Tag::TargetCompID, "OMS");
    nos.setField(oms::fix::Tag::ClOrdID,      clOrdId);
    nos.setField(oms::fix::Tag::Symbol,       symbol);
    nos.setField(oms::fix::Tag::Side,         side);
    nos.setField(oms::fix::Tag::OrdType,      ordType);
    nos.setField(oms::fix::Tag::TimeInForce,  static_cast<char>(oms::fix::TimeInForce::Day));
    nos.setField(oms::fix::Tag::OrderQty,     qty, 0);
    nos.setField(oms::fix::Tag::Price,        price, 4);
    nos.setField(oms::fix::Tag::HandlInst,    static_cast<char>(oms::fix::HandlInst::AutomatedNoIntervention));
    nos.setField(oms::fix::Tag::Account,      "ACCT-001");
    return nos;
}

static oms::fix::FixMessage makeCancelReq(const std::string& newClOrdId,
                                          const std::string& origClOrdId,
                                          const std::string& symbol,
                                          char               side) {
    oms::fix::FixMessage req(oms::fix::MsgType::OrderCancelRequest);
    req.setField(oms::fix::Tag::MsgSeqNum,    g_seqNum++);
    req.setField(oms::fix::Tag::SenderCompID, "CLIENT1");
    req.setField(oms::fix::Tag::TargetCompID, "OMS");
    req.setField(oms::fix::Tag::ClOrdID,      newClOrdId);
    req.setField(41,                          origClOrdId);  // OrigClOrdID
    req.setField(oms::fix::Tag::Symbol,       symbol);
    req.setField(oms::fix::Tag::Side,         side);
    return req;
}

int main() {
    std::cout << "======================================\n";
    std::cout << "  Bloomberg OMS — End-to-End Demo\n";
    std::cout << "======================================\n\n";

    // Configure engine
    oms::OmsEngine::Config cfg;
    cfg.d_verbose = true;
    cfg.d_riskConfig.d_maxOrderNotional    = 5000000.0;
    cfg.d_riskConfig.d_maxPositionNotional = 50000000.0;
    cfg.d_riskConfig.d_maxDailyLoss        = -1000000.0;
    cfg.d_riskConfig.d_maxOrderQty         = 500000;
    cfg.d_riskConfig.d_maxOpenOrders       = 500;
    cfg.d_exchangeConfig.d_fillRatePct     = 85.0;
    cfg.d_exchangeConfig.d_minLatencyUs    = 50;
    cfg.d_exchangeConfig.d_maxLatencyUs    = 300;

    oms::OrderBus bus;
    oms::OmsEngine engine(cfg, bus);

    engine.start();
    std::cout << "[DEMO] OMS engine started.\n\n";
    msleep(50);

    // ── Scenario 1: Normal buy limit order ──
    std::cout << "--- Scenario 1: Buy limit order (AAPL) ---\n";
    bus.pushInbound(makeNOS("ORD001", "AAPL",
                            oms::fix::Side::Buy, 500, 185.50));
    msleep(400);

    // ── Scenario 2: Sell limit order ──
    std::cout << "\n--- Scenario 2: Sell limit order (IBM) ---\n";
    bus.pushInbound(makeNOS("ORD002", "IBM",
                            oms::fix::Side::Sell, 200, 141.00));
    msleep(400);

    // ── Scenario 3: Risk breach (oversized notional) ──
    std::cout << "\n--- Scenario 3: Risk reject (notional breach) ---\n";
    bus.pushInbound(makeNOS("ORD003", "MSFT",
                            oms::fix::Side::Buy, 100000, 380.00));
    msleep(200);

    // ── Scenario 4: Cancel request ──
    std::cout << "\n--- Scenario 4: Cancel order (ORD001) ---\n";
    bus.pushInbound(makeCancelReq("CXL001", "ORD001", "AAPL",
                                  oms::fix::Side::Buy));
    msleep(200);

    // ── Scenario 5: Market order ──
    std::cout << "\n--- Scenario 5: Market buy order (GOOG) ---\n";
    bus.pushInbound(makeNOS("ORD004", "GOOG",
                            oms::fix::Side::Buy, 100, 0.0,
                            oms::fix::OrdType::Market));
    msleep(400);

    // ── Scenario 6: Burst of orders ──
    std::cout << "\n--- Scenario 6: Burst (10 orders) ---\n";
    for (int i = 5; i <= 14; ++i) {
        std::ostringstream id;
        id << "ORD" << std::setw(3) << std::setfill('0') << i;
        const char* sym = (i % 2 == 0) ? "AMZN" : "NVDA";
        char side = (i % 3 == 0) ? oms::fix::Side::Sell : oms::fix::Side::Buy;
        bus.pushInbound(makeNOS(id.str(), sym, side, 100 + i * 10, 150.00 + i));
    }
    msleep(1500);

    engine.stop();

    std::cout << "\n======================================\n";
    std::cout << "  Session Summary\n";
    std::cout << "--------------------------------------\n";
    std::cout << "  Orders processed : " << engine.ordersProcessed() << "\n";
    std::cout << "  Fills received   : " << engine.fillsReceived()   << "\n";
    std::cout << "  Rejects issued   : " << engine.rejectsIssued()   << "\n";
    std::cout << "======================================\n";

    return 0;
}
