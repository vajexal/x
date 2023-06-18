#include "pass.h"

#include <vector>

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

        std::vector<llvm::IntrinsicInst *> roots;

        // collect roots
        for (auto &BB: F) {
            for (auto &instruction: BB) {
                if (auto intrinsic = llvm::dyn_cast<llvm::IntrinsicInst>(&instruction)) {
                    if (intrinsic->getIntrinsicID() == llvm::Intrinsic::gcroot) {
                        roots.push_back(intrinsic);
                    }
                }
            }
        }

        if (roots.empty()) {
            return false;
        }

        // push stack frame
        llvm::IRBuilder<> builder(&F.getEntryBlock(), F.getEntryBlock().begin());
        builder.SetInsertPointPastAllocas(&F);
        builder.CreateCall(gcPushStackFrameFn, {gcVar});

        // replace gcroot intrinsic
        for (auto root: roots) {
            llvm::ReplaceInstWithInst(root, llvm::CallInst::Create(gcAddRootFn, {gcVar, root->getArgOperand(0), root->getArgOperand(1)}));
        }

        // pop stack frame
        for (auto &BB: F) {
            if (auto terminator = BB.getTerminator()) {
                if (llvm::isa<llvm::ReturnInst>(terminator)) {
                    llvm::CallInst::Create(gcPopStackFrameFn, {gcVar}, "", terminator);
                }
            }
        }

        return true;
    }
}
