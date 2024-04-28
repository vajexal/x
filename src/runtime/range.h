#pragma once

#include <string>

#include "llvm/IR/Type.h"

namespace X::Runtime {
    struct Range {
        static inline const std::string CLASS_NAME = "Range";

        int64_t start;
        int64_t stop;
        int64_t step;
    };

    int64_t Range_length(Range *that);
    int64_t Range_get(Range *that, int64_t idx);
}
