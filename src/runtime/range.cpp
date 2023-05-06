#include "range.h"

#include <cmath>

#include "utils.h"

namespace X::Runtime {
    bool Range::isRangeType(llvm::Type *type) {
        type = deref(type);
        return type->isStructTy() && type->getStructName() == Runtime::Range::CLASS_NAME;
    }

    int64_t Range_length(Range *that) {
        auto dist = that->stop - that->start;
        if ((dist > 0 && that->step < 0) || (dist < 0 && that->step > 0)) {
            return 0;
        }

        return std::ceil((double)dist / that->step);
    }

    int64_t Range_get(Range *that, int64_t idx) {
        // not most effective implementation, but that will do
        // todo optimize
        return that->start + that->step * idx;
    }
}
