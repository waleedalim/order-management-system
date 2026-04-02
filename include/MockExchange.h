#ifndef OMS_MOCK_EXCHANGE_H
#define OMS_MOCK_EXCHANGE_H

#include "Order.h"
#include "FixMessage.h"
#include <string>
#include <vector>
#include <pthread.h>

namespace oms {

// FillEvent — returned by the mock exchange after processing an order.
struct FillEvent {
    std::string d_orderId;
    std::string d_execId;
    double      d_fillQty;
    double      d_fillPx;
    bool        d_isFinal;   // true = fully filled or rejected
    bool        d_rejected;
    std::string d_rejectReason;
};

// MockExchange — simulates an exchange matching engine.
// Randomly fills orders with configurable fill rate and latency.
// Supports partial fills (splits order into 1–3 partial fills).
class MockExchange {
  public:
    struct Config {
        double d_fillRatePct;      // 0–100: probability order gets filled
        int    d_minLatencyUs;     // min simulated exchange latency (microseconds)
        int    d_maxLatencyUs;     // max simulated exchange latency
        double d_priceSlippagePct; // max % slippage from limit price

        Config()
            : d_fillRatePct(85.0)
            , d_minLatencyUs(50)
            , d_maxLatencyUs(500)
            , d_priceSlippagePct(0.05)
        {}
    };

    explicit MockExchange(const Config& config);

    // Submit an order to the mock exchange.
    // Returns one or more FillEvents (partial or full fill, or reject).
    std::vector<FillEvent> processOrder(const Order& order);

    // Generate an ExecutionReport FIX message for a fill event.
    fix::FixMessage buildExecutionReport(const Order&     order,
                                        const FillEvent& fill,
                                        int              seqNum) const;

  private:
    Config  d_config;
    int     d_execCounter;
    pthread_mutex_t d_mutex;

    double  simulateFillPrice(const Order& order) const;
    std::string nextExecId();

    MockExchange(const MockExchange&);
    MockExchange& operator=(const MockExchange&);
};

} // namespace oms

#endif // OMS_MOCK_EXCHANGE_H
