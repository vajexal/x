#pragma once

#include "llvm/IR/PassManager.h"

#include "mangler.h"

namespace X::GC {
    class XGCLowering : public llvm::PassInfoMixin<XGCLowering> {
    public:
        llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);
    };
}
