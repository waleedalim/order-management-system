#include "RiskEngine.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>

namespace oms {

RiskConfig::RiskConfig()
    : d_maxOrderNotional(5000000.0)   // $5M per order
    , d_maxPositionNotional(50000000.0) // $50M per symbol
    , d_maxDailyLoss(-1000000.0)       // -$1M daily loss limit
    , d_maxOrderQty(1000000)            // 1M shares
    , d_maxOpenOrders(500)
{}

bool RiskConfig::loadFromFile(const std::string& path, std::string& errorOut) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) {
        errorOut = "Cannot open risk config: " + path;
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "max_order_notional")    d_maxOrderNotional    = std::atof(val.c_str());
        if (key == "max_position_notional") d_maxPositionNotional = std::atof(val.c_str());
        if (key == "max_daily_loss")        d_maxDailyLoss        = std::atof(val.c_str());
        if (key == "max_order_qty")         d_maxOrderQty         = std::atoi(val.c_str());
        if (key == "max_open_orders")       d_maxOpenOrders       = std::atoi(val.c_str());
    }
    return true;
}

RiskEngine::RiskEngine(const RiskConfig& config, PositionManager& positionMgr)
    : d_config(config), d_positionMgr(positionMgr) {}

RiskCheckResult RiskEngine::checkOrder(const Order& order) const {
    RiskCheckResult r = checkOrderQty(order);
    if (!r.d_passed) return r;

    r = checkNotional(order);
    if (!r.d_passed) return r;

    r = checkPosition(order);
    if (!r.d_passed) return r;

    r = checkDailyLoss();
    if (!r.d_passed) return r;

    return RiskCheckResult(true);
}

RiskCheckResult RiskEngine::checkNotional(const Order& order) const {
    // For market orders price is 0 — use last known price from position mgr
    double px = order.d_price;
    if (px <= 0.0) {
        SymbolPosition pos = d_positionMgr.getPosition(order.d_symbol);
        px = pos.d_lastPx;
    }
    double notional = order.d_orderQty * px;
    if (notional > d_config.d_maxOrderNotional) {
        std::ostringstream oss;
        oss << "Order notional $" << notional
            << " exceeds limit $" << d_config.d_maxOrderNotional;
        return RiskCheckResult(false, oss.str());
    }
    return RiskCheckResult(true);
}

RiskCheckResult RiskEngine::checkPosition(const Order& order) const {
    SymbolPosition pos = d_positionMgr.getPosition(order.d_symbol);
    double px = (order.d_price > 0.0) ? order.d_price : pos.d_lastPx;
    double projectedNotional = (std::fabs(pos.d_netQty) + order.d_orderQty) * px;
    if (projectedNotional > d_config.d_maxPositionNotional) {
        std::ostringstream oss;
        oss << "Projected position notional $" << projectedNotional
            << " exceeds limit $" << d_config.d_maxPositionNotional;
        return RiskCheckResult(false, oss.str());
    }
    return RiskCheckResult(true);
}

RiskCheckResult RiskEngine::checkOrderQty(const Order& order) const {
    if (order.d_orderQty > d_config.d_maxOrderQty) {
        std::ostringstream oss;
        oss << "Order qty " << order.d_orderQty
            << " exceeds max " << d_config.d_maxOrderQty;
        return RiskCheckResult(false, oss.str());
    }
    return RiskCheckResult(true);
}

RiskCheckResult RiskEngine::checkOpenOrders(int currentOpenCount) const {
    if (currentOpenCount >= d_config.d_maxOpenOrders) {
        std::ostringstream oss;
        oss << "Max open orders (" << d_config.d_maxOpenOrders << ") reached";
        return RiskCheckResult(false, oss.str());
    }
    return RiskCheckResult(true);
}

RiskCheckResult RiskEngine::checkDailyLoss() const {
    double pnl = d_positionMgr.totalRealizedPnl();
    if (pnl < d_config.d_maxDailyLoss) {
        std::ostringstream oss;
        oss << "Daily loss $" << pnl
            << " breaches limit $" << d_config.d_maxDailyLoss;
        return RiskCheckResult(false, oss.str());
    }
    return RiskCheckResult(true);
}

} // namespace oms
