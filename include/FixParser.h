#ifndef OMS_FIX_PARSER_H
#define OMS_FIX_PARSER_H

#include "FixMessage.h"
#include <string>

namespace oms {
namespace fix {

// FixParser — parses raw SOH-delimited FIX wire strings into FixMessage.
// Returns false on malformed input without throwing.
class FixParser {
  public:
    // Parse a complete FIX message string (SOH-delimited).
    // Returns true and populates 'out' on success.
    // Returns false and sets 'errorOut' on failure.
    static bool parse(const std::string& raw,
                      FixMessage&        out,
                      std::string&       errorOut);

    // Validate checksum of a raw FIX string.
    static bool validateChecksum(const std::string& raw);

  private:
    static bool parseField(const std::string& token,
                           int&               tagOut,
                           std::string&       valueOut);
};

} // namespace fix
} // namespace oms

#endif // OMS_FIX_PARSER_H
