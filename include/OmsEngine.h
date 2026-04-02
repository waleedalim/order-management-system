#ifndef OMS_ENGINE_H
#define OMS_ENGINE_H

#include "OrderBook.h"
#include "RiskEngine.h"
#include "PositionManager.h"
#include "MockExchange.h"
#include "OrderBus.h"
#include "FixMessage.h"
#include <string>
#include <pthread.h>

namespace oms {

// OmsEngine — central coordinator.
// Owns the order book, risk engine, position manager, and exchange connector.
// Processes inbound FIX messages from the bus and dispatches fills back.
class OmsEngine {
  public:
    struct Config {
        RiskConfig        d_riskConfig;
        MockExchange::Config d_exchangeConfig;
        bool              d_verbose;
        Config() : d_verbose(true) {}
    };

    explicit OmsEngine(const Config& config, OrderBus& bus);
    ~OmsEngine();

    // Start the engine processing thread.
    void start();

    // Signal the engine to stop and join the thread.
    void stop();

    // Statistics
    std::size_t ordersProcessed() const;
    std::size_t fillsReceived()   const;
    std::size_t rejectsIssued()   const;

    // Direct (non-bus) submission for benchmarking
    bool submitDirect(const fix::FixMessage& nos);

  private:
    static void* threadFunc(void* arg);
    void processLoop();
    void handleNewOrderSingle(const fix::FixMessage& msg);
    void handleCancelRequest(const fix::FixMessage& msg);
    void publishExecutionReport(const fix::FixMessage& er);

    Config          d_config;
    OrderBus&       d_bus;
    OrderBook       d_orderBook;
    PositionManager d_positionMgr;
    RiskEngine      d_riskEngine;
    MockExchange    d_exchange;

    pthread_t       d_thread;
    volatile bool   d_running;
    std::size_t     d_ordersProcessed;
    std::size_t     d_fillsReceived;
    std::size_t     d_rejectsIssued;
    int             d_outSeqNum;

    OmsEngine(const OmsEngine&);
    OmsEngine& operator=(const OmsEngine&);
};

} // namespace oms

#endif // OMS_ENGINE_H
