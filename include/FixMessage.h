#ifndef OMS_FIX_MESSAGE_H
#define OMS_FIX_MESSAGE_H

#include "FixConstants.h"
#include <string>
#include <map>
#include <sstream>
#include <stdint.h>

namespace oms {
namespace fix {

// FixMessage — represents a single FIX protocol message.
// Field storage uses std::map<int,std::string> for simplicity.
// Hot-path serialization avoids heap allocation where possible.
class FixMessage {
  public:
    FixMessage();
    explicit FixMessage(const std::string& msgType);

    // Field accessors
    void        setField(int tag, const std::string& value);
    void        setField(int tag, int value);
    void        setField(int tag, double value, int precision = 4);
    void        setField(int tag, char value);

    bool        hasField(int tag) const;
    std::string getField(int tag) const;       // returns "" if missing
    int         getFieldAsInt(int tag) const;  // returns 0 if missing
    double      getFieldAsDbl(int tag) const;  // returns 0.0 if missing
    char        getFieldAsChar(int tag) const; // returns '\0' if missing

    // Serialise to FIX wire format (SOH-delimited)
    std::string toString() const;

    // Clear all fields
    void clear();

    std::string getMsgType() const;

  private:
    std::map<int, std::string> d_fields;

    static std::string computeChecksum(const std::string& body);
    static int         sumBytes(const std::string& s);
};

} // namespace fix
} // namespace oms

#endif // OMS_FIX_MESSAGE_H
