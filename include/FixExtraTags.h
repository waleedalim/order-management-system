// Patch: add LastPx (tag 31) and LastQty (tag 32) and OrigClOrdID (tag 41)
// to FixConstants.h Tag::Enum. This companion header extends Tag for
// fields used in ExecutionReports and cancel requests.

#ifndef OMS_FIX_EXTRA_TAGS_H
#define OMS_FIX_EXTRA_TAGS_H

namespace oms {
namespace fix {

// Extra tags not in the core Tag::Enum (defined here to keep FixConstants.h clean)
struct ExtraTag {
    enum Enum {
        OrigClOrdID = 41,
        LastPx      = 31,
        LastQty     = 32
    };
};

} // namespace fix
} // namespace oms

#endif // OMS_FIX_EXTRA_TAGS_H
