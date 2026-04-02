#include "PositionManager.h"
#include <cmath>

namespace oms {

SymbolPosition::SymbolPosition()
    : d_longQty(0.0), d_shortQty(0.0), d_netQty(0.0)
    , d_realizedPnl(0.0), d_lastPx(0.0) {}

double SymbolPosition::notional() const {
    return std::fabs(d_netQty) * d_lastPx;
}

double SymbolPosition::unrealizedPnl(double currentPx) const {
    return d_netQty * (currentPx - d_lastPx);
}

PositionManager::PositionManager() {
    pthread_mutex_init(&d_mutex, NULL);
}

PositionManager::~PositionManager() {
    pthread_mutex_destroy(&d_mutex);
}

void PositionManager::onFill(const std::string& symbol,
                             fix::Side::Enum    side,
                             double             qty,
                             double             fillPx) {
    pthread_mutex_lock(&d_mutex);

    SymbolPosition& pos = d_positions[symbol];
    pos.d_symbol = symbol;

    if (side == fix::Side::Buy) {
        // If we have short positions, realize P&L on the cover
        double cover = std::min(qty, pos.d_shortQty);
        if (cover > 0.0) {
            // Short cover P&L: sold at higher price, buying back cheaper = profit
            pos.d_realizedPnl += cover * (pos.d_lastPx - fillPx);
            pos.d_shortQty    -= cover;
            qty               -= cover;
        }
        pos.d_longQty += qty;
    } else {
        // Sell / SellShort
        double closeOut = std::min(qty, pos.d_longQty);
        if (closeOut > 0.0) {
            pos.d_realizedPnl += closeOut * (fillPx - pos.d_lastPx);
            pos.d_longQty     -= closeOut;
            qty               -= closeOut;
        }
        pos.d_shortQty += qty;
    }

    pos.d_netQty  = pos.d_longQty - pos.d_shortQty;
    pos.d_lastPx  = fillPx;

    pthread_mutex_unlock(&d_mutex);
}

void PositionManager::updateMarketPrice(const std::string& symbol, double px) {
    pthread_mutex_lock(&d_mutex);
    d_positions[symbol].d_lastPx = px;
    pthread_mutex_unlock(&d_mutex);
}

SymbolPosition PositionManager::getPosition(const std::string& symbol) const {
    pthread_mutex_lock(&d_mutex);
    std::map<std::string, SymbolPosition>::const_iterator it =
        d_positions.find(symbol);
    SymbolPosition result;
    if (it != d_positions.end()) result = it->second;
    pthread_mutex_unlock(&d_mutex);
    return result;
}

double PositionManager::totalRealizedPnl() const {
    pthread_mutex_lock(&d_mutex);
    double total = 0.0;
    for (std::map<std::string, SymbolPosition>::const_iterator it =
             d_positions.begin(); it != d_positions.end(); ++it) {
        total += it->second.d_realizedPnl;
    }
    pthread_mutex_unlock(&d_mutex);
    return total;
}

double PositionManager::totalNotional() const {
    pthread_mutex_lock(&d_mutex);
    double total = 0.0;
    for (std::map<std::string, SymbolPosition>::const_iterator it =
             d_positions.begin(); it != d_positions.end(); ++it) {
        total += it->second.notional();
    }
    pthread_mutex_unlock(&d_mutex);
    return total;
}

} // namespace oms
