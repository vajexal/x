#include "range.h"

#include <cmath>

namespace X::Runtime {
    Range *Range_create(int64_t start, int64_t stop, int64_t step) {
        if (step == 0) {
            std::abort(); // todo nice error
        }

        return new Range{start, stop, step};
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
