#include "FixParser.h"
#include "FixConstants.h"
#include <sstream>
#include <cstdlib>
#include <iomanip>

namespace oms {
namespace fix {

bool FixParser::parse(const std::string& raw,
                      FixMessage&        out,
                      std::string&       errorOut) {
    out.clear();
    if (raw.empty()) {
        errorOut = "Empty message";
        return false;
    }

    std::string token;
    for (std::size_t i = 0; i <= raw.size(); ++i) {
        char c = (i < raw.size()) ? raw[i] : FIX_DELIM;
        if (c == FIX_DELIM) {
            if (!token.empty()) {
                int tag = 0;
                std::string value;
                if (!parseField(token, tag, value)) {
                    errorOut = "Malformed field: " + token;
                    return false;
                }
                out.setField(tag, value);
                token.clear();
            }
        } else {
            token += c;
        }
    }

    // Minimal validation: must have BeginString and MsgType
    if (!out.hasField(Tag::BeginString)) {
        errorOut = "Missing BeginString (tag 8)";
        return false;
    }
    if (!out.hasField(Tag::MsgType)) {
        errorOut = "Missing MsgType (tag 35)";
        return false;
    }

    return true;
}

bool FixParser::parseField(const std::string& token,
                           int&               tagOut,
                           std::string&       valueOut) {
    std::size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0) return false;
    std::string tagStr = token.substr(0, eq);
    tagOut   = std::atoi(tagStr.c_str());
    valueOut = token.substr(eq + 1);
    return (tagOut > 0);
}

bool FixParser::validateChecksum(const std::string& raw) {
    // Find last SOH before checksum field
    std::size_t csPos = raw.rfind('\x01', raw.size() - 2);
    if (csPos == std::string::npos) return false;
    std::size_t bodyEnd = csPos + 1;

    // Sum all bytes up to and including the SOH before "10="
    std::string body = raw.substr(0, bodyEnd);
    int sum = 0;
    for (std::size_t i = 0; i < body.size(); ++i)
        sum += static_cast<unsigned char>(body[i]);
    int computed = sum % 256;

    // Extract checksum from tag 10
    std::size_t csTagPos = raw.find("10=", bodyEnd);
    if (csTagPos == std::string::npos) return false;
    int provided = std::atoi(raw.c_str() + csTagPos + 3);

    return (computed == provided);
}

} // namespace fix
} // namespace oms
