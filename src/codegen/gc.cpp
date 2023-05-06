#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gcAlloc(llvm::Value *size) {
        auto allocFn = module.getFunction(mangler.mangleInternalFunction("gcAlloc"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        return builder.CreateCall(allocFn, {gcVar, size});
    }

    void Codegen::gcPushStackFrame() {
        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcPushStackFrame"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        builder.CreateCall(gcAddRootFn, {gcVar});
    }

    void Codegen::gcPopStackFrame() {
        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcPopStackFrame"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        builder.CreateCall(gcAddRootFn, {gcVar});
    }

    void Codegen::gcAddRoot(llvm::Value *root) {
        auto meta = getValueGCMeta(root);
        if (!meta) {
            return;
        }

        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcAddRoot"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));
        auto rootVoidPtr = builder.CreateBitCast(root, builder.getInt8PtrTy()->getPointerTo());

        builder.CreateCall(gcAddRootFn, {gcVar, rootVoidPtr, meta});
    }
}
