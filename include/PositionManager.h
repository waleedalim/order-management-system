#ifndef OMS_POSITION_MANAGER_H
#define OMS_POSITION_MANAGER_H

#include "FixConstants.h"
#include <string>
#include <map>
#include <pthread.h>

namespace oms {

// SymbolPosition — real-time position state for a single symbol.
struct SymbolPosition {
    std::string d_symbol;
    double      d_longQty;      // total long shares
    double      d_shortQty;     // total short shares
    double      d_netQty;       // longQty - shortQty
    double      d_realizedPnl;  // from closed fills
    double      d_lastPx;       // last known market price

    SymbolPosition();
    double notional() const;    // abs(netQty) * lastPx
    double unrealizedPnl(double currentPx) const;
};

// PositionManager — tracks real-time P&L and positions across all symbols.
// Updated on every fill event. Thread-safe.
class PositionManager {
  public:
    PositionManager();
    ~PositionManager();

    // Called after a fill is confirmed by the exchange.
    void onFill(const std::string&    symbol,
                fix::Side::Enum       side,
                double                qty,
                double                fillPx);

    // Update last known price for unrealised P&L calc.
    void updateMarketPrice(const std::string& symbol, double px);

    // Query position for a symbol (returns default if unknown).
    SymbolPosition getPosition(const std::string& symbol) const;

    // Sum of all realised P&L (for daily loss limit check).
    double totalRealizedPnl() const;

    // Gross notional exposure across all symbols.
    double totalNotional() const;

  private:
    mutable pthread_mutex_t d_mutex;
    std::map<std::string, SymbolPosition> d_positions;

    PositionManager(const PositionManager&);
    PositionManager& operator=(const PositionManager&);
};

} // namespace oms

#endif // OMS_POSITION_MANAGER_H
