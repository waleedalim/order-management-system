#ifndef OMS_FIX_CONSTANTS_H
#define OMS_FIX_CONSTANTS_H

// Bloomberg OMS Project — FIX 4.2/4.4 Constants
// Style: BDE-aligned, C++11, no exceptions on hot path

namespace oms {
namespace fix {

// FIX message type tags
struct MsgType {
    static const char* NewOrderSingle;      // "D"
    static const char* OrderCancelRequest;  // "F"
    static const char* OrderCancelReplace;  // "G"
    static const char* ExecutionReport;     // "8"
    static const char* Heartbeat;           // "0"
    static const char* Logon;               // "A"
    static const char* Logout;              // "5"
};

// FIX field tag numbers (most common subset)
struct Tag {
    enum Enum {
        BeginString      = 8,
        BodyLength       = 9,
        MsgType          = 35,
        SenderCompID     = 49,
        TargetCompID     = 56,
        MsgSeqNum        = 34,
        SendingTime      = 52,
        CheckSum         = 10,

        // Order fields
        ClOrdID          = 11,
        OrderID          = 37,
        ExecID           = 17,
        Symbol           = 55,
        Side             = 54,
        OrderQty         = 38,
        OrdType          = 40,
        Price            = 44,
        TimeInForce      = 59,
        TransactTime     = 60,
        OrdStatus        = 39,
        ExecType         = 150,
        LeavesQty        = 151,
        CumQty           = 14,
        AvgPx            = 6,
        Text             = 58,
        Account          = 1,
        HandlInst        = 21
    };
};

// Side values
struct Side {
    enum Enum {
        Buy  = '1',
        Sell = '2',
        SellShort = '5'
    };
};

// OrdType values
struct OrdType {
    enum Enum {
        Market          = '1',
        Limit           = '2',
        Stop            = '3',
        StopLimit       = '4'
    };
};

// OrdStatus values
struct OrdStatus {
    enum Enum {
        New             = '0',
        PartiallyFilled = '1',
        Filled          = '2',
        Canceled        = '4',
        PendingCancel   = '6',
        Rejected        = '8',
        PendingNew      = 'A',
        PendingReplace  = 'E'
    };
};

// ExecType values
struct ExecType {
    enum Enum {
        New             = '0',
        PartialFill     = '1',
        Fill            = '2',
        Canceled        = '4',
        Rejected        = '8',
        PendingNew      = 'A'
    };
};

// TimeInForce values
struct TimeInForce {
    enum Enum {
        Day             = '0',
        GoodTillCancel  = '1',
        ImmediateOrCancel = '3',
        FillOrKill      = '4'
    };
};

// HandlInst values
struct HandlInst {
    enum Enum {
        AutomatedNoIntervention = '1',
        AutomatedWithIntervention = '2',
        Manual = '3'
    };
};

const char FIX_DELIM = '\x01';  // SOH character

} // namespace fix
} // namespace oms

#endif // OMS_FIX_CONSTANTS_H
