#include "utils.h"

#include "ast.h"

namespace X {
    llvm::Type *deref(llvm::Type *type) {
        if (type->isPointerTy()) {
            return type->getPointerElementType();
        }

        return type;
    }
}
