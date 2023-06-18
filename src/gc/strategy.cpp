#include "strategy.h"

namespace X::GC {
    static llvm::GCRegistry::Add<XGCStrategy> X("x", "Mark-and-sweep gc");
}

