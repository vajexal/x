#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gcAlloc(llvm::Value *size) {
        auto allocFn = module.getFunction(mangler.mangleInternalFunction("gcAlloc"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        return builder.CreateCall(allocFn, {gcVar, size});
    }

    void Codegen::gcAddRoot(llvm::Value *root) {
        auto meta = getValueGCMeta(root);
        if (!meta) {
            return;
        }

        auto rootVoidPtr = builder.CreateBitCast(root, builder.getInt8PtrTy()->getPointerTo());
        builder.CreateIntrinsic(llvm::Intrinsic::gcroot, {}, {rootVoidPtr, meta});
    }
}
