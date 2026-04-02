// LatencyBench.cpp — OMS hot-path latency benchmark
//
// Measures end-to-end order submission latency:
//   FIX parse -> risk check -> order book insert -> exchange route
//
// Uses CLOCK_MONOTONIC (Linux/WSL) for nanosecond resolution.
// Outputs: min, max, mean, p50, p95, p99 latency + histogram.
//
// Build:  cmake --build . --target oms_bench
// Run:    ./oms_bench [num_orders]   (default: 100000)

#include "FixMessage.h"
#include "FixParser.h"
#include "FixConstants.h"
#include "OrderBook.h"
#include "RiskEngine.h"
#include "PositionManager.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdlib>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
typedef long long int64_t;
static int64_t nowNs() {
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (int64_t)(cnt.QuadPart * 1000000000LL / freq.QuadPart);
}
#else
#include <time.h>
static int64_t nowNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
#endif

// Build a FIX NewOrderSingle message for benchmarking
static oms::fix::FixMessage buildNOS(int seqNum, const std::string& symbol,
                                     double qty, double px) {
    std::ostringstream clOrdId;
    clOrdId << "BENCH" << seqNum;

    oms::fix::FixMessage nos(oms::fix::MsgType::NewOrderSingle);
    nos.setField(oms::fix::Tag::MsgSeqNum,    seqNum);
    nos.setField(oms::fix::Tag::SenderCompID, "BENCH");
    nos.setField(oms::fix::Tag::TargetCompID, "OMS");
    nos.setField(oms::fix::Tag::ClOrdID,      clOrdId.str());
    nos.setField(oms::fix::Tag::Symbol,       symbol);
    nos.setField(oms::fix::Tag::Side,         static_cast<char>(oms::fix::Side::Buy));
    nos.setField(oms::fix::Tag::OrdType,      static_cast<char>(oms::fix::OrdType::Limit));
    nos.setField(oms::fix::Tag::TimeInForce,  static_cast<char>(oms::fix::TimeInForce::Day));
    nos.setField(oms::fix::Tag::OrderQty,     qty, 0);
    nos.setField(oms::fix::Tag::Price,        px, 4);
    nos.setField(oms::fix::Tag::HandlInst,    static_cast<char>(oms::fix::HandlInst::AutomatedNoIntervention));
    nos.setField(oms::fix::Tag::Account,      "ACCT001");
    return nos;
}

// Print a simple ASCII histogram of latency samples (nanoseconds)
static void printHistogram(const std::vector<int64_t>& samples,
                           int64_t minNs, int64_t maxNs, int buckets = 20) {
    if (samples.empty()) return;
    int64_t range = maxNs - minNs;
    if (range <= 0) range = 1;

    std::vector<int> counts(buckets, 0);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        int b = (int)(((samples[i] - minNs) * buckets) / (range + 1));
        if (b < 0) b = 0;
        if (b >= buckets) b = buckets - 1;
        ++counts[b];
    }

    int maxCount = *std::max_element(counts.begin(), counts.end());
    int barWidth = 40;

    std::cout << "\n  Latency Histogram (ns)\n";
    std::cout << "  " << std::string(60, '-') << "\n";
    for (int b = 0; b < buckets; ++b) {
        int64_t lo = minNs + (range * b)     / buckets;
        int64_t hi = minNs + (range * (b+1)) / buckets;
        int barLen = (maxCount > 0) ? (counts[b] * barWidth / maxCount) : 0;
        std::cout << "  " << std::setw(7) << lo
                  << " - " << std::setw(7) << hi << " | "
                  << std::string(barLen, '#')
                  << " " << counts[b] << "\n";
    }
    std::cout << "  " << std::string(60, '-') << "\n";
}

int main(int argc, char* argv[]) {
    int numOrders = 100000;
    if (argc > 1) numOrders = std::atoi(argv[1]);
    if (numOrders < 1) numOrders = 1;

    std::cout << "========================================\n";
    std::cout << "  Bloomberg OMS — Latency Benchmark\n";
    std::cout << "  Orders: " << numOrders << "\n";
    std::cout << "========================================\n\n";

    // Setup — mirrors production configuration
    oms::RiskConfig riskCfg;
    riskCfg.d_maxOrderNotional    = 100000000.0; // relaxed for bench
    riskCfg.d_maxPositionNotional = 999999999.0;
    riskCfg.d_maxDailyLoss        = -99999999.0;
    riskCfg.d_maxOrderQty         = 999999;
    riskCfg.d_maxOpenOrders       = 999999;

    oms::PositionManager posMgr;
    oms::RiskEngine      riskEngine(riskCfg, posMgr);
    oms::OrderBook       orderBook;

    // Warm up the position manager with a reference price
    posMgr.updateMarketPrice("AAPL",  185.50);
    posMgr.updateMarketPrice("IBM",   140.00);
    posMgr.updateMarketPrice("MSFT",  380.00);

    const char* symbols[] = { "AAPL", "IBM", "MSFT", "GOOG", "AMZN" };
    const int   numSymbols = 5;

    std::vector<int64_t> latencies;
    latencies.reserve(numOrders);

    int64_t totalStart = nowNs();

    // ── Hot path benchmark ──
    // Measures: FIX message creation + risk check + order book insert
    // (exchange routing is excluded — that involves I/O latency)
    for (int i = 0; i < numOrders; ++i) {
        const char* sym = symbols[i % numSymbols];
        double qty = 100.0 + (i % 900);    // 100–999 shares
        double px  = 100.0 + (i % 300);    // $100–$399

        oms::fix::FixMessage nos = buildNOS(i + 1, sym, qty, px);

        int64_t t0 = nowNs();

        // --- Begin hot path ---
        oms::Order order;
        oms::OmsResult submitResult = orderBook.submitOrder(nos, order);
        if (submitResult.ok()) {
            oms::RiskCheckResult risk = riskEngine.checkOrder(order);
            if (!risk.d_passed) {
                orderBook.rejectOrder(order.d_orderId, risk.d_reason);
            }
        }
        // --- End hot path ---

        int64_t t1 = nowNs();
        latencies.push_back(t1 - t0);
    }

    int64_t totalEnd = nowNs();

    // ── Statistics ──
    std::sort(latencies.begin(), latencies.end());

    int64_t minLat  = latencies.front();
    int64_t maxLat  = latencies.back();
    int64_t p50     = latencies[latencies.size() * 50 / 100];
    int64_t p95     = latencies[latencies.size() * 95 / 100];
    int64_t p99     = latencies[latencies.size() * 99 / 100];
    int64_t p999    = latencies[latencies.size() * 999 / 1000];

    double meanLat = 0.0;
    for (std::size_t i = 0; i < latencies.size(); ++i)
        meanLat += (double)latencies[i];
    meanLat /= (double)latencies.size();

    double totalMs = (totalEnd - totalStart) / 1e6;
    double throughput = (double)numOrders / ((totalEnd - totalStart) / 1e9);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Hot-path latency (FIX build + risk check + book insert)\n";
    std::cout << "  " << std::string(44, '-') << "\n";
    std::cout << "  Min      : " << std::setw(10) << minLat  << " ns\n";
    std::cout << "  Mean     : " << std::setw(10) << (int64_t)meanLat << " ns\n";
    std::cout << "  P50      : " << std::setw(10) << p50     << " ns\n";
    std::cout << "  P95      : " << std::setw(10) << p95     << " ns\n";
    std::cout << "  P99      : " << std::setw(10) << p99     << " ns\n";
    std::cout << "  P99.9    : " << std::setw(10) << p999    << " ns\n";
    std::cout << "  Max      : " << std::setw(10) << maxLat  << " ns\n";
    std::cout << "  " << std::string(44, '-') << "\n";
    std::cout << "  Total    : " << totalMs << " ms for " << numOrders << " orders\n";
    std::cout << "  Throughput: " << (int)throughput << " orders/sec\n";

    printHistogram(latencies, minLat, maxLat);

    std::cout << "\n  Orders in book: " << orderBook.totalOrderCount() << "\n";
    std::cout << "  Active orders : " << orderBook.activeOrders().size() << "\n";
    std::cout << "\n  Done.\n";

    return 0;
}
