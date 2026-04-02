// tests.cpp — Unit tests for OMS core modules
// Self-contained: no external test framework required.
// Build: cmake --build . --target oms_tests
// Run:   ./oms_tests

#include "FixMessage.h"
#include "FixParser.h"
#include "FixConstants.h"
#include "Order.h"
#include "OrderBook.h"
#include "RiskEngine.h"
#include "PositionManager.h"

#include <iostream>
#include <cassert>
#include <string>
#include <sstream>
#include <cmath>

// ── Test framework (minimal) ──────────────────────────────────────────────

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    static void name(); \
    struct name##_reg { name##_reg() { \
        std::cout << "  Running " #name " ... "; \
        try { name(); std::cout << "PASS\n"; ++g_passed; } \
        catch (const std::exception& e) { \
            std::cout << "FAIL: " << e.what() << "\n"; ++g_failed; } \
        catch (...) { std::cout << "FAIL (unknown)\n"; ++g_failed; } \
    } } name##_instance; \
    static void name()

#define ASSERT_TRUE(cond) \
    do { if (!(cond)) { \
        std::ostringstream _oss; \
        _oss << "ASSERT_TRUE failed: " #cond " at line " << __LINE__; \
        throw std::runtime_error(_oss.str()); \
    } } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { \
        std::ostringstream _oss; \
        _oss << "ASSERT_EQ failed: [" << (a) << "] != [" << (b) \
             << "] at line " << __LINE__; \
        throw std::runtime_error(_oss.str()); \
    } } while(0)

#define ASSERT_NEAR(a, b, eps) \
    do { if (std::fabs((double)(a) - (double)(b)) > (eps)) { \
        std::ostringstream _oss; \
        _oss << "ASSERT_NEAR failed: " << (a) << " vs " << (b) \
             << " (eps=" << (eps) << ") at line " << __LINE__; \
        throw std::runtime_error(_oss.str()); \
    } } while(0)

// ── FIX Message Tests ──────────────────────────────────────────────────────

TEST(test_fix_message_set_get_string) {
    oms::fix::FixMessage msg;
    msg.setField(oms::fix::Tag::Symbol, "AAPL");
    ASSERT_EQ(msg.getField(oms::fix::Tag::Symbol), "AAPL");
}

TEST(test_fix_message_set_get_int) {
    oms::fix::FixMessage msg;
    msg.setField(oms::fix::Tag::MsgSeqNum, 42);
    ASSERT_EQ(msg.getFieldAsInt(oms::fix::Tag::MsgSeqNum), 42);
}

TEST(test_fix_message_set_get_double) {
    oms::fix::FixMessage msg;
    msg.setField(oms::fix::Tag::Price, 185.5, 4);
    ASSERT_NEAR(msg.getFieldAsDbl(oms::fix::Tag::Price), 185.5, 0.0001);
}

TEST(test_fix_message_set_get_char) {
    oms::fix::FixMessage msg;
    msg.setField(oms::fix::Tag::Side, static_cast<char>(oms::fix::Side::Buy));
    ASSERT_EQ(msg.getFieldAsChar(oms::fix::Tag::Side), '1');
}

TEST(test_fix_message_missing_field_returns_empty) {
    oms::fix::FixMessage msg;
    ASSERT_EQ(msg.getField(999), "");
    ASSERT_EQ(msg.getFieldAsInt(999), 0);
    ASSERT_NEAR(msg.getFieldAsDbl(999), 0.0, 0.0001);
}

TEST(test_fix_message_serialise_has_soh) {
    oms::fix::FixMessage msg("D");
    msg.setField(oms::fix::Tag::ClOrdID, "ORD001");
    msg.setField(oms::fix::Tag::Symbol,  "AAPL");
    std::string wire = msg.toString();
    ASSERT_TRUE(wire.find('\x01') != std::string::npos);
    ASSERT_TRUE(wire.find("35=D") != std::string::npos);
    ASSERT_TRUE(wire.find("55=AAPL") != std::string::npos);
}

// ── FIX Parser Tests ───────────────────────────────────────────────────────

TEST(test_fix_parser_roundtrip) {
    oms::fix::FixMessage orig("D");
    orig.setField(oms::fix::Tag::MsgSeqNum,    1);
    orig.setField(oms::fix::Tag::SenderCompID, "CLIENT");
    orig.setField(oms::fix::Tag::TargetCompID, "OMS");
    orig.setField(oms::fix::Tag::ClOrdID,      "ORD001");
    orig.setField(oms::fix::Tag::Symbol,       "IBM");
    orig.setField(oms::fix::Tag::Side,         static_cast<char>(oms::fix::Side::Buy));
    orig.setField(oms::fix::Tag::OrderQty,     100.0, 0);
    orig.setField(oms::fix::Tag::Price,        140.0, 4);

    std::string wire = orig.toString();

    oms::fix::FixMessage parsed;
    std::string err;
    bool ok = oms::fix::FixParser::parse(wire, parsed, err);
    ASSERT_TRUE(ok);
    ASSERT_EQ(parsed.getMsgType(), "D");
    ASSERT_EQ(parsed.getField(oms::fix::Tag::Symbol),  "IBM");
    ASSERT_EQ(parsed.getField(oms::fix::Tag::ClOrdID), "ORD001");
}

TEST(test_fix_parser_rejects_empty) {
    oms::fix::FixMessage out;
    std::string err;
    bool ok = oms::fix::FixParser::parse("", out, err);
    ASSERT_TRUE(!ok);
    ASSERT_TRUE(!err.empty());
}

TEST(test_fix_parser_rejects_missing_msgtype) {
    // Build a wire string with tag 8 but no tag 35
    std::string wire = "8=FIX.4.2\x01""9=5\x01""10=000\x01";
    oms::fix::FixMessage out;
    std::string err;
    bool ok = oms::fix::FixParser::parse(wire, out, err);
    ASSERT_TRUE(!ok);
}

// ── OrderBook Tests ────────────────────────────────────────────────────────

static oms::fix::FixMessage makeTestNOS(const std::string& clOrdId,
                                        const std::string& symbol = "AAPL",
                                        double qty = 100.0,
                                        double px  = 185.0) {
    oms::fix::FixMessage nos("D");
    nos.setField(oms::fix::Tag::ClOrdID,     clOrdId);
    nos.setField(oms::fix::Tag::Symbol,      symbol);
    nos.setField(oms::fix::Tag::Side,        static_cast<char>(oms::fix::Side::Buy));
    nos.setField(oms::fix::Tag::OrdType,     static_cast<char>(oms::fix::OrdType::Limit));
    nos.setField(oms::fix::Tag::TimeInForce, static_cast<char>(oms::fix::TimeInForce::Day));
    nos.setField(oms::fix::Tag::OrderQty,    qty, 0);
    nos.setField(oms::fix::Tag::Price,       px, 4);
    nos.setField(oms::fix::Tag::Account,     "ACCT");
    return nos;
}

TEST(test_orderbook_submit_success) {
    oms::OrderBook book;
    oms::fix::FixMessage nos = makeTestNOS("ORD001");
    oms::Order order;
    oms::OmsResult r = book.submitOrder(nos, order);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(order.d_clOrdId, "ORD001");
    ASSERT_EQ(order.d_symbol,  "AAPL");
    ASSERT_NEAR(order.d_orderQty, 100.0, 0.01);
    ASSERT_EQ(order.d_state, oms::OrderState::New);
}

TEST(test_orderbook_duplicate_clordid_rejected) {
    oms::OrderBook book;
    oms::Order o;
    book.submitOrder(makeTestNOS("DUP001"), o);
    oms::OmsResult r2 = book.submitOrder(makeTestNOS("DUP001"), o);
    ASSERT_TRUE(!r2.ok());
    ASSERT_EQ(r2.d_code, oms::OmsResult::DuplicateOrder);
}

TEST(test_orderbook_apply_partial_fill) {
    oms::OrderBook book;
    oms::Order o;
    book.submitOrder(makeTestNOS("FILL001", "AAPL", 200.0, 185.0), o);

    oms::OmsResult r = book.applyFill(o.d_orderId, 100.0, 185.0);
    ASSERT_TRUE(r.ok());

    oms::Order updated;
    book.findByOrderId(o.d_orderId, updated);
    ASSERT_EQ(updated.d_state, oms::OrderState::PartiallyFilled);
    ASSERT_NEAR(updated.d_cumQty,    100.0, 0.01);
    ASSERT_NEAR(updated.d_leavesQty, 100.0, 0.01);
    ASSERT_NEAR(updated.d_avgPx,     185.0, 0.01);
}

TEST(test_orderbook_apply_full_fill) {
    oms::OrderBook book;
    oms::Order o;
    book.submitOrder(makeTestNOS("FILL002", "IBM", 100.0, 140.0), o);
    book.applyFill(o.d_orderId, 100.0, 140.0);

    oms::Order updated;
    book.findByOrderId(o.d_orderId, updated);
    ASSERT_EQ(updated.d_state, oms::OrderState::Filled);
    ASSERT_NEAR(updated.d_leavesQty, 0.0, 0.01);
    ASSERT_TRUE(updated.isTerminal());
}

TEST(test_orderbook_cancel_active_order) {
    oms::OrderBook book;
    oms::Order o;
    book.submitOrder(makeTestNOS("CXL001"), o);
    oms::OmsResult r = book.cancelOrder("CXL001", "CXL001");
    ASSERT_TRUE(r.ok());

    oms::Order updated;
    book.findByClOrdId("CXL001", updated);
    ASSERT_EQ(updated.d_state, oms::OrderState::Canceled);
}

TEST(test_orderbook_cannot_cancel_filled_order) {
    oms::OrderBook book;
    oms::Order o;
    book.submitOrder(makeTestNOS("FLD001"), o);
    book.applyFill(o.d_orderId, 100.0, 185.0);  // fully fill

    oms::OmsResult r = book.cancelOrder("FLD001", "FLD001");
    ASSERT_TRUE(!r.ok());
}

TEST(test_orderbook_reject_order) {
    oms::OrderBook book;
    oms::Order o;
    book.submitOrder(makeTestNOS("REJ001"), o);
    oms::OmsResult r = book.rejectOrder(o.d_orderId, "Risk limit");
    ASSERT_TRUE(r.ok());

    oms::Order updated;
    book.findByOrderId(o.d_orderId, updated);
    ASSERT_EQ(updated.d_state, oms::OrderState::Rejected);
}

// ── Risk Engine Tests ──────────────────────────────────────────────────────

static oms::Order makeTestOrder(const std::string& symbol = "AAPL",
                                double qty = 100.0, double px = 185.0) {
    oms::Order o;
    o.d_clOrdId   = "TEST";
    o.d_orderId   = "ORD0";
    o.d_symbol    = symbol;
    o.d_side      = oms::fix::Side::Buy;
    o.d_ordType   = oms::fix::OrdType::Limit;
    o.d_orderQty  = qty;
    o.d_price     = px;
    o.d_leavesQty = qty;
    o.d_state     = oms::OrderState::New;
    return o;
}

TEST(test_risk_passes_normal_order) {
    oms::RiskConfig cfg;
    oms::PositionManager pm;
    oms::RiskEngine re(cfg, pm);

    oms::Order o = makeTestOrder("AAPL", 100.0, 185.0);
    oms::RiskCheckResult r = re.checkOrder(o);
    ASSERT_TRUE(r.d_passed);
}

TEST(test_risk_rejects_oversized_notional) {
    oms::RiskConfig cfg;
    cfg.d_maxOrderNotional = 1000.0;  // $1000 limit
    oms::PositionManager pm;
    pm.updateMarketPrice("AAPL", 185.0);
    oms::RiskEngine re(cfg, pm);

    oms::Order o = makeTestOrder("AAPL", 100.0, 185.0);  // $18500 notional
    oms::RiskCheckResult r = re.checkOrder(o);
    ASSERT_TRUE(!r.d_passed);
    ASSERT_TRUE(!r.d_reason.empty());
}

TEST(test_risk_rejects_oversized_qty) {
    oms::RiskConfig cfg;
    cfg.d_maxOrderQty = 50;
    oms::PositionManager pm;
    oms::RiskEngine re(cfg, pm);

    oms::Order o = makeTestOrder("AAPL", 100.0, 185.0);
    oms::RiskCheckResult r = re.checkOrder(o);
    ASSERT_TRUE(!r.d_passed);
}

// ── Position Manager Tests ─────────────────────────────────────────────────

TEST(test_position_manager_long_fill) {
    oms::PositionManager pm;
    pm.onFill("AAPL", oms::fix::Side::Buy, 100.0, 185.0);

    oms::SymbolPosition pos = pm.getPosition("AAPL");
    ASSERT_NEAR(pos.d_longQty,  100.0, 0.01);
    ASSERT_NEAR(pos.d_shortQty, 0.0,   0.01);
    ASSERT_NEAR(pos.d_netQty,   100.0, 0.01);
}

TEST(test_position_manager_realize_pnl_on_sell) {
    oms::PositionManager pm;
    pm.onFill("IBM", oms::fix::Side::Buy,  100.0, 140.0);
    pm.onFill("IBM", oms::fix::Side::Sell, 100.0, 145.0);  // $5 profit

    oms::SymbolPosition pos = pm.getPosition("IBM");
    ASSERT_NEAR(pos.d_netQty,       0.0,  0.01);
    ASSERT_NEAR(pos.d_realizedPnl, 500.0, 0.01);  // 100 * ($145 - $140)
}

TEST(test_position_manager_total_realized_pnl) {
    oms::PositionManager pm;
    pm.onFill("AAPL", oms::fix::Side::Buy,  50.0, 180.0);
    pm.onFill("AAPL", oms::fix::Side::Sell, 50.0, 185.0);  // +$250
    pm.onFill("IBM",  oms::fix::Side::Buy,  100.0, 142.0);
    pm.onFill("IBM",  oms::fix::Side::Sell, 100.0, 140.0);  // -$200
    ASSERT_NEAR(pm.totalRealizedPnl(), 50.0, 0.01);
}

// ── Order FSM Tests ────────────────────────────────────────────────────────

TEST(test_order_state_terminal_flags) {
    oms::Order o;
    o.d_state = oms::OrderState::Filled;
    ASSERT_TRUE(o.isTerminal());
    ASSERT_TRUE(!o.isActive());

    o.d_state = oms::OrderState::PartiallyFilled;
    ASSERT_TRUE(!o.isTerminal());
    ASSERT_TRUE(o.isActive());

    o.d_state = oms::OrderState::Canceled;
    ASSERT_TRUE(o.isTerminal());
}

// ── Entry point ────────────────────────────────────────────────────────────

int main() {
    std::cout << "========================================\n";
    std::cout << "  Bloomberg OMS — Unit Tests\n";
    std::cout << "========================================\n";

    std::cout << "\nFIX Message:\n";
    // Tests registered by static constructors above — already ran at this point

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << g_passed << " passed, "
              << g_failed << " failed\n";
    std::cout << "========================================\n";

    return (g_failed == 0) ? 0 : 1;
}
