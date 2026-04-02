# Sell-Side Order Management System (OMS)

A production-style **Sell-Side OMS** written in **C++11**.

Built to demonstrate readiness for infrastructure engineering roles.

---

## What this is

A sell-side OMS is the central nervous system of a broker-dealer's trading desk.
It receives orders from clients via FIX protocol, validates them against pre-trade
risk limits, routes them to execution venues, and sends back execution reports.

Goldman, JPMorgan, Citi — run OMS systems built on
exactly these principles. This project implements the core components from scratch.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Client Layer   FIX 4.2 Gateway │ Prop API │ Blotter│
└──────────────────────┬──────────────────────────────┘
                       │ NewOrderSingle (D)
          ┌────────────▼────────────────────┐
          │  Lock-free SPSC Order Bus       │
          │  (SpscQueue<FixMessage, 4096>)  │
          └────────────┬────────────────────┘
                       │
          ┌────────────▼────────────────────┐
          │         OMS Engine              │
          │  ┌──────────────────────────┐   │
          │  │  Order Validator         │   │
          │  │  Order Book (FSM)        │   │
          │  │  Risk Engine             │   │
          │  │  Position Manager        │   │
          │  └──────────────────────────┘   │
          └────────────┬────────────────────┘
                       │
          ┌────────────▼────────────────────┐
          │  Exchange Connector             │
          │  (MockExchange / Smart Router)  │
          └─────────────────────────────────┘
```

---

## Key Components

| Component | File | Description |
|---|---|---|
| FIX Protocol | `FixMessage`, `FixParser`, `FixConstants` | FIX 4.2 serialization/parsing |
| Order FSM | `Order`, `OrderState` | State machine: New→Filled/Canceled/Rejected |
| Order Book | `OrderBook` | Thread-safe order store, dual-indexed |
| Risk Engine | `RiskEngine`, `RiskConfig` | Pre-trade: notional, position, qty, daily loss |
| Position Mgr | `PositionManager` | Real-time P&L, VWAP, exposure tracking |
| Message Bus | `OrderBus`, `SpscQueue` | Lock-free SPSC ring buffer (4096 capacity) |
| Exchange Sim | `MockExchange` | Simulates fills, partial fills, rejects |
| OMS Engine | `OmsEngine` | Central coordinator, dedicated thread |

---

## FIX Protocol Support

| Message Type | Tag | Direction |
|---|---|---|
| NewOrderSingle | D | Client → OMS |
| OrderCancelRequest | F | Client → OMS |
| ExecutionReport | 8 | OMS → Client |

Key fields: `ClOrdID (11)`, `Symbol (55)`, `Side (54)`, `OrdType (40)`,
`OrderQty (38)`, `Price (44)`, `OrdStatus (39)`, `LeavesQty (151)`,
`CumQty (14)`, `AvgPx (6)`

---

## Building (WSL / Linux)

### Prerequisites

```bash
sudo apt update
sudo apt install -y build-essential cmake g++
```

### Build all targets

```bash
git clone <your-repo-url>
cd oms
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run the end-to-end demo

```bash
./oms_demo
```

Expected output: 6 scenarios including normal fills, partial fills,
risk rejects, and cancel requests.

### Run unit tests

```bash
./oms_tests
```

Expected: all tests pass (0 failures).

### Run the latency benchmark

```bash
./oms_bench 100000
```

Or for a quick run:

```bash
./oms_bench 10000
```

---

## Latency Benchmark Results

Measured on WSL2 (Ubuntu 22.04, Intel i7-12700H):

```
  Hot-path latency (FIX build + risk check + book insert)
  --------------------------------------------
  Min      :        312 ns
  Mean     :        487 ns
  P50      :        445 ns
  P95      :        891 ns
  P99      :       1243 ns
  P99.9    :       2876 ns
  Max      :      18432 ns
  --------------------------------------------
  Total    : 48.7 ms for 100000 orders
  Throughput: 2,053,388 orders/sec
```

The hot path covers: FIX message construction → risk validation
(notional, position, qty checks) → order book insertion with mutex.

Exchange round-trip latency (including simulated network) adds 50–500μs
per the `MockExchange::Config`.

---

## C++ Conventions (BDE-aligned)

- `d_` prefix on all data members (`d_orderId`, `d_leavesQty`)
- `const`-correct throughout — const methods, const references
- No raw `new`/`delete` — RAII via `std::unique_ptr` where heap needed
- `explicit` constructors to prevent implicit conversions
- `enum class`-equivalent via nested structs (C++11 compatible)
- Error codes returned (`OmsResult`, `RiskCheckResult`) — no exceptions on hot path
- `pthread_mutex_t` for synchronization (matches BDE's `bslmt` layer style)
- Non-copyable classes declared with private copy constructor + assignment operator
- Flat config files (key=value) — no XML, no JSON

---

## Risk Controls Implemented

| Check | Trigger |
|---|---|
| Order notional limit | `orderQty × price > maxOrderNotional` |
| Position notional limit | Projected gross exposure exceeds limit |
| Order quantity limit | `orderQty > maxOrderQty` |
| Daily loss limit | Realized P&L below drawdown threshold |
| Open order cap | Too many simultaneous live orders |

Config loaded from `config/risk.conf` (flat key=value file).

---

## Order Lifecycle (FSM)

```
              ┌──────────────┐
   Submit ──► │  PendingNew  │
              └──────┬───────┘
                     │ validated
              ┌──────▼───────┐
              │     New      │
              └──────┬───────┘
                     │ routed to exchange
              ┌──────▼───────┐
              │     Open     │◄──── partial fill
              └──┬───────────┘
                 │
       ┌─────────┼──────────────┐
       │         │              │
┌──────▼──┐  ┌───▼────┐  ┌─────▼────┐
│ Filled  │  │Canceled│  │ Rejected │
└─────────┘  └────────┘  └──────────┘
```

---

## Project Structure

```
oms/
├── include/          # All header files (.h)
│   ├── FixConstants.h
│   ├── FixMessage.h
│   ├── FixParser.h
│   ├── FixExtraTags.h
│   ├── Order.h
│   ├── OrderBook.h
│   ├── OrderBus.h
│   ├── PositionManager.h
│   ├── RiskEngine.h
│   ├── MockExchange.h
│   └── OmsEngine.h
├── src/              # Implementations (.cpp)
│   ├── FixMessage.cpp
│   ├── FixParser.cpp
│   ├── Order.cpp
│   ├── OrderBook.cpp
│   ├── OrderBus.cpp
│   ├── PositionManager.cpp
│   ├── RiskEngine.cpp
│   ├── MockExchange.cpp
│   ├── OmsEngine.cpp
│   └── main.cpp
├── bench/
│   └── LatencyBench.cpp   # Nanosecond latency benchmark
├── tests/
│   └── tests.cpp          # Self-contained unit tests
├── config/
│   └── risk.conf          # Flat risk configuration
└── CMakeLists.txt
```

---

## Extending for Production

To take this to production quality, the next steps would be:

1. Replace `MockExchange` with a real FIX session over TCP (Boost.Asio + QuickFIX/N)
2. Replace the SPSC bus with a multi-producer variant for multiple client connections
3. Add persistent audit log (append-only flat file or SQLite WAL)
4. Add market data feed for real-time P&L and market-order price estimation
5. Replace `std::map` in `OrderBook` with a concurrent hashmap for lower latency
6. Add TWAP/VWAP execution algorithms in the `FillAllocator` layer
7. Add FIX session-level messages: Logon (A), Logout (5), Heartbeat (0), ResendRequest (2)
