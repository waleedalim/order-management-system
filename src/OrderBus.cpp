#include "OrderBus.h"

namespace oms {

bool OrderBus::pushInbound(const fix::FixMessage& msg)  { return d_inbound.push(msg);   }
bool OrderBus::popInbound(fix::FixMessage& out)          { return d_inbound.pop(out);    }
bool OrderBus::pushOutbound(const fix::FixMessage& msg)  { return d_outbound.push(msg);  }
bool OrderBus::popOutbound(fix::FixMessage& out)         { return d_outbound.pop(out);   }
bool OrderBus::inboundEmpty()  const                     { return d_inbound.empty();     }
bool OrderBus::outboundEmpty() const                     { return d_outbound.empty();    }

} // namespace oms
