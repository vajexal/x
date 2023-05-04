#include "utils.h"

#include "ast.h"

namespace X {
    llvm::Type *deref(llvm::Type *type) {
        while (type->isPointerTy()) {
            type = type->getPointerElementType();
        }

        return type;
    }
}
