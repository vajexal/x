#include "utils.h"

#include "ast.h"

namespace X {
    llvm::Type *deref(llvm::Type *type) {
        while (type->isPointerTy()) {
            type = type->getPointerElementType();
        }

        return type;
    }

    llvm::ConstantInt *getTypeSize(llvm::Module &module, llvm::Type *type) {
        auto typeSize = module.getDataLayout().getTypeAllocSize(type);
        if (typeSize.isScalable()) {
            throw std::runtime_error("can't calc obj size");
        }
        auto size = typeSize.getFixedSize();
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(module.getContext()), size);
    }
}
