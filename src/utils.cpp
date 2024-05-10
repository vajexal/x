#include "utils.h"

#include "ast.h"

namespace X {
    llvm::ConstantInt *getTypeSize(llvm::Module &module, llvm::Type *type) {
        auto typeSize = module.getDataLayout().getTypeAllocSize(type);
        if (typeSize.isScalable()) {
            throw std::runtime_error("can't calc obj size");
        }
        auto size = typeSize.getFixedValue();
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(module.getContext()), size);
    }

    void die(const char *s) {
        std::cout << s << std::endl;
        std::exit(1);
    }
}
