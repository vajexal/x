#include "pass.h"

#include <vector>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace X::GC {
    llvm::PreservedAnalyses XGCLowering::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
        auto M = F.getParent();
        auto gcPushStackFrameFn = M->getFunction(Mangler::mangleInternalFunction("gcPushStackFrame"));
        auto gcPopStackFrameFn = M->getFunction(Mangler::mangleInternalFunction("gcPopStackFrame"));
        auto gcAddRootFn = M->getFunction(Mangler::mangleInternalFunction("gcAddRoot"));
        auto gcVar = M->getGlobalVariable(Mangler::mangleInternalSymbol("gc"));

        if (!F.hasGC() || F.getGC() != "x") {
            return llvm::PreservedAnalyses::all();
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
            return llvm::PreservedAnalyses::all();
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

        llvm::PreservedAnalyses PA;
        PA.preserve<llvm::DominatorTreeAnalysis>();
        return PA;
    }
}
