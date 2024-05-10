#pragma once

#include "llvm/IR/PassManager.h"

#include "mangler.h"

namespace X::GC {
    class XGCLowering : public llvm::PassInfoMixin<XGCLowering> {
        std::shared_ptr<Mangler> mangler;

    public:
        XGCLowering(std::shared_ptr<Mangler> mangler) : mangler(std::move(mangler)) {}

        llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM);
    };
}
