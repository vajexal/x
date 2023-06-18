#pragma once

#include "llvm/IR/GCStrategy.h"

namespace X::GC {
    class XGCStrategy : public llvm::GCStrategy {
    public:
        XGCStrategy() = default;
    };
}
