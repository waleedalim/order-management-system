#include "FixMessage.h"
#include "FixConstants.h"
#include <sstream>
#include <iomanip>
#include <cstdlib>

namespace oms {
namespace fix {

const char* MsgType::NewOrderSingle     = "D";
const char* MsgType::OrderCancelRequest = "F";
const char* MsgType::OrderCancelReplace = "G";
const char* MsgType::ExecutionReport    = "8";
const char* MsgType::Heartbeat          = "0";
const char* MsgType::Logon              = "A";
const char* MsgType::Logout             = "5";

FixMessage::FixMessage() {}

FixMessage::FixMessage(const std::string& msgType) {
    setField(Tag::BeginString, "FIX.4.2");
    setField(Tag::MsgType, msgType);
}

void FixMessage::setField(int tag, const std::string& value) {
    d_fields[tag] = value;
}

void FixMessage::setField(int tag, int value) {
    std::ostringstream oss;
    oss << value;
    d_fields[tag] = oss.str();
}

void FixMessage::setField(int tag, double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    d_fields[tag] = oss.str();
}

void FixMessage::setField(int tag, char value) {
    d_fields[tag] = std::string(1, value);
}

bool FixMessage::hasField(int tag) const {
    return d_fields.find(tag) != d_fields.end();
}

std::string FixMessage::getField(int tag) const {
    std::map<int,std::string>::const_iterator it = d_fields.find(tag);
    if (it == d_fields.end()) return "";
    return it->second;
}

int FixMessage::getFieldAsInt(int tag) const {
    std::string v = getField(tag);
    if (v.empty()) return 0;
    return std::atoi(v.c_str());
}

double FixMessage::getFieldAsDbl(int tag) const {
    std::string v = getField(tag);
    if (v.empty()) return 0.0;
    return std::atof(v.c_str());
}

char FixMessage::getFieldAsChar(int tag) const {
    std::string v = getField(tag);
    if (v.empty()) return '\0';
    return v[0];
}

std::string FixMessage::getMsgType() const {
    return getField(Tag::MsgType);
}

void FixMessage::clear() {
    d_fields.clear();
}

// Build body string (all tags except 8, 9, 10 in tag order)
std::string FixMessage::toString() const {
    // Build body: all fields except BeginString, BodyLength, CheckSum
    std::ostringstream body;
    for (std::map<int,std::string>::const_iterator it = d_fields.begin();
         it != d_fields.end(); ++it) {
        int tag = it->first;
        if (tag == Tag::BeginString || tag == Tag::BodyLength || tag == Tag::CheckSum)
            continue;
        body << tag << '=' << it->second << FIX_DELIM;
    }
    std::string bodyStr = body.str();

    // Prepend standard header fields
    std::ostringstream msg;
    msg << Tag::BeginString << '=' << getField(Tag::BeginString) << FIX_DELIM;
    msg << Tag::BodyLength  << '=' << bodyStr.size() << FIX_DELIM;
    msg << bodyStr;

    std::string partial = msg.str();
    int cs = sumBytes(partial) % 256;
    std::ostringstream csStr;
    csStr << std::setw(3) << std::setfill('0') << cs;
    msg << Tag::CheckSum << '=' << csStr.str() << FIX_DELIM;

    return msg.str();
}

int FixMessage::sumBytes(const std::string& s) {
    int sum = 0;
    for (std::size_t i = 0; i < s.size(); ++i)
        sum += static_cast<unsigned char>(s[i]);
    return sum;
}

std::string FixMessage::computeChecksum(const std::string& body) {
    int cs = sumBytes(body) % 256;
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << cs;
    return oss.str();
}

} // namespace fix
} // namespace oms
