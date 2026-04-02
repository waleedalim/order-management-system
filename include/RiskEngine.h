#ifndef OMS_RISK_ENGINE_H
#define OMS_RISK_ENGINE_H

#include "Order.h"
#include "PositionManager.h"
#include <string>
#include <map>

namespace oms {

// RiskConfig — loaded from flat file at startup.
// Bloomberg style: no XML, no JSON — simple key=value.
struct RiskConfig {
    double d_maxOrderNotional;    // Max notional per single order (USD)
    double d_maxPositionNotional; // Max gross position per symbol (USD)
    double d_maxDailyLoss;        // Max daily P&L drawdown (USD, negative)
    int    d_maxOrderQty;         // Max shares per order
    int    d_maxOpenOrders;       // Max simultaneous open orders

    RiskConfig();

    // Load from flat file (key=value, one per line, # comments)
    bool loadFromFile(const std::string& path, std::string& errorOut);
};

// RiskCheckResult — returned by pre-trade checks.
struct RiskCheckResult {
    bool        d_passed;
    std::string d_reason;

    explicit RiskCheckResult(bool passed, const std::string& reason = "")
        : d_passed(passed), d_reason(reason) {}
};

// RiskEngine — pre-trade risk validation.
// All checks run synchronously on the order submission path.
class RiskEngine {
  public:
    explicit RiskEngine(const RiskConfig& config,
                        PositionManager&  positionMgr);

    // Run all pre-trade checks against a proposed order.
    // Must be called before submitting to OrderBook.
    RiskCheckResult checkOrder(const Order& order) const;

  private:
    RiskCheckResult checkNotional(const Order& order) const;
    RiskCheckResult checkPosition(const Order& order) const;
    RiskCheckResult checkOrderQty(const Order& order) const;
    RiskCheckResult checkOpenOrders(int currentOpenCount) const;
    RiskCheckResult checkDailyLoss() const;

    const RiskConfig&  d_config;
    PositionManager&   d_positionMgr;
};

} // namespace oms

#endif // OMS_RISK_ENGINE_H
