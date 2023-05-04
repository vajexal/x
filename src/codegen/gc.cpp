#include "codegen.h"

#include "utils.h"

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

    void Codegen::gcAddRoot(llvm::Value *root) {
        unsigned long classId;
        auto type = deref(root->getType());

        if (Runtime::String::isStringType(type)) {
            classId = Runtime::GC::GC::STRING_CLASS_ID;
        } else if (isInterfaceType(type)) {
            classId = Runtime::GC::GC::INTERFACE_CLASS_ID;
        } else if (isClassType(type)) {
            classId = getClassIdByMangledName(type->getStructName().str());
        } else {
            return;
        }

        auto gcAddRootFn = module.getFunction(mangler.mangleInternalFunction("gcAddRoot"));
        auto gc = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));
        auto rootVoidPtr = builder.CreateBitCast(root, builder.getInt8PtrTy()->getPointerTo());

        builder.CreateCall(gcAddRootFn, {gc, rootVoidPtr, builder.getInt64(classId)});
    }
}
