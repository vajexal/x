#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gcAlloc(llvm::Value *size) {
        auto allocFn = module.getFunction(Mangler::mangleInternalFunction("gcAlloc"));
        auto gcVar = module.getGlobalVariable(Mangler::mangleInternalSymbol("gc"));

        return builder.CreateCall(allocFn, {gcVar, size});
    }

    void Codegen::gcAddRoot(llvm::Value *root, const Type &type) {
        auto meta = getGCMetaValue(type);
        if (!meta) {
            return;
        }

        builder.CreateIntrinsic(llvm::Intrinsic::gcroot, {}, {root, meta});
    }
}
