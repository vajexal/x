#include "codegen.h"

namespace X::Codegen {
    llvm::Value *Codegen::gcAlloc(llvm::Value *size) {
        auto allocFn = module.getFunction(mangler.mangleInternalFunction("gcAlloc"));
        auto gc = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        return builder.CreateCall(allocFn, {gc, size});
    }

    void Codegen::gcPushStackFrame() {
        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcPushStackFrame"));
        auto gc = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        builder.CreateCall(gcAddRootFn, {gc});
    }

    void Codegen::gcPopStackFrame() {
        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcPopStackFrame"));
        auto gc = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        builder.CreateCall(gcAddRootFn, {gc});
    }

    void Codegen::gcAddRoot(llvm::Value *root, unsigned long classId) {
        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcAddRoot"));
        auto gc = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));

        builder.CreateCall(gcAddRootFn, {gc, root, builder.getInt64(classId)});
    }
}
