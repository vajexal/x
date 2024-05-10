#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gcAlloc(llvm::Value *size) {
        auto allocFn = module.getFunction(mangler->mangleInternalFunction("gcAlloc"));
        auto gcVar = module.getGlobalVariable(mangler->mangleInternalSymbol("gc"));

        return builder.CreateCall(allocFn, {gcVar, size});
    }

    void Codegen::gcAddRoot(llvm::AllocaInst *root, const Type &type) {
        auto meta = getGCMetaValue(type);
        if (!meta) {
            return;
        }

        builder.CreateIntrinsic(llvm::Intrinsic::gcroot, {}, {root, meta});
    }

    void Codegen::gcAddGlobalRoot(llvm::Value *root, const Type &type) {
        auto meta = getGCMetaValue(type);
        if (!meta) {
            return;
        }

        auto gcVar = module.getGlobalVariable(mangler->mangleInternalSymbol("gc"));
        builder.CreateCall(module.getFunction(mangler->mangleInternalFunction("gcAddGlobalRoot")), {gcVar, root, meta});
    }
}
