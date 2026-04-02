#ifndef OMS_ORDER_BUS_H
#define OMS_ORDER_BUS_H

// SpscQueue — Single-Producer Single-Consumer lock-free ring buffer.
// Uses C++11 std::atomic for memory ordering.
// Sized at compile time with a power-of-2 capacity for fast modulo.

#include "FixMessage.h"
#include <atomic>
#include <vector>
#include <stdint.h>

namespace oms {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");
  public:
    SpscQueue() : d_head(0), d_tail(0) {
        d_buf.resize(Capacity);
    }

    // Push from producer thread. Returns false if full.
    bool push(const T& item) {
        std::size_t head = d_head.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) & (Capacity - 1);
        if (next == d_tail.load(std::memory_order_acquire))
            return false; // full
        d_buf[head] = item;
        d_head.store(next, std::memory_order_release);
        return true;
    }

    // Pop from consumer thread. Returns false if empty.
    bool pop(T& out) {
        std::size_t tail = d_tail.load(std::memory_order_relaxed);
        if (tail == d_head.load(std::memory_order_acquire))
            return false; // empty
        out = d_buf[tail];
        d_tail.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return d_head.load(std::memory_order_acquire) ==
               d_tail.load(std::memory_order_acquire);
    }

  private:
    std::atomic<std::size_t> d_head;
    std::atomic<std::size_t> d_tail;
    std::vector<T>           d_buf;

    SpscQueue(const SpscQueue&);
    SpscQueue& operator=(const SpscQueue&);
};

// OrderBus — wraps two SPSC queues:
//   inbound:  FIX messages from gateway → OMS engine
//   outbound: ExecutionReports from OMS engine → gateway
static const std::size_t BUS_CAPACITY = 4096;

class OrderBus {
  public:
    // Producer (gateway) side — push inbound FIX message
    bool pushInbound(const fix::FixMessage& msg);

    // Consumer (engine) side — pop next inbound FIX message
    bool popInbound(fix::FixMessage& out);

    // Producer (engine) side — push outbound execution report
    bool pushOutbound(const fix::FixMessage& msg);

    // Consumer (gateway) side — pop outbound execution report
    bool popOutbound(fix::FixMessage& out);

    bool inboundEmpty()  const;
    bool outboundEmpty() const;

  private:
    SpscQueue<fix::FixMessage, BUS_CAPACITY> d_inbound;
    SpscQueue<fix::FixMessage, BUS_CAPACITY> d_outbound;
};

} // namespace oms

#endif // OMS_ORDER_BUS_H
