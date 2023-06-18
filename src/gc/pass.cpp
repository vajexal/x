#include "pass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace X::GC {
    bool XGCLowering::doInitialization(llvm::Module &M) {
        gcPushStackFrameFn = M.getFunction(mangler.mangleInternalFunction("gcPushStackFrame"));
        gcPopStackFrameFn = M.getFunction(mangler.mangleInternalFunction("gcPopStackFrame"));
        gcAddRootFn = M.getFunction(mangler.mangleInternalFunction("gcAddRoot"));
        gcVar = M.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        return false;
    }

    bool XGCLowering::runOnFunction(llvm::Function &F) {
        if (!F.hasGC() || F.getGC() != "x") {
            return false;
        }

        // push stack frame
        llvm::IRBuilder<> builder(&F.getEntryBlock(), F.getEntryBlock().begin());
        builder.SetInsertPointPastAllocas(&F);
        builder.CreateCall(gcPushStackFrameFn, {gcVar});

        for (auto &BB: F) {
            // replace gcroot intrinsic
            for (auto it = BB.begin(); it != BB.end();) {
                auto &instruction = *it;
                it++;

                if (auto intrinsic = llvm::dyn_cast<llvm::IntrinsicInst>(&instruction)) {
                    if (intrinsic->getIntrinsicID() == llvm::Intrinsic::gcroot) {
                        llvm::ReplaceInstWithInst(
                                intrinsic,
                                llvm::CallInst::Create(gcAddRootFn, {gcVar, intrinsic->getArgOperand(0), intrinsic->getArgOperand(1)})
                        );
                    }
                }
            }

            // pop stack frame
            if (auto terminator = BB.getTerminator()) {
                if (llvm::isa<llvm::ReturnInst>(terminator)) {
                    llvm::CallInst::Create(gcPopStackFrameFn, {gcVar}, "", terminator);
                }
            }
        }

        return true;
    }
}
