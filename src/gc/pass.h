#pragma once

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include "mangler.h"

namespace X::GC {
    class XGCLowering : public llvm::FunctionPass {
        Mangler mangler;

        llvm::Function *gcPushStackFrameFn;
        llvm::Function *gcPopStackFrameFn;
        llvm::Function *gcAddRootFn;
        llvm::GlobalVariable *gcVar;

    public:
        static inline char ID = 0;

        XGCLowering() : llvm::FunctionPass(ID) {}

        bool doInitialization(llvm::Module &M) override;
        bool runOnFunction(llvm::Function &F) override;
    };
}
