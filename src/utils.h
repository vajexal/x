#ifndef X_UTILS_H
#define X_UTILS_H

#include "llvm/IR/Type.h"

namespace X {
    class MethodDeclNode;
    class MethodDefNode;

    bool compareDeclAndDef(MethodDeclNode *declNode, MethodDefNode *defNode);
    llvm::Type *deref(llvm::Type *type);

    template<typename To, typename From>
    bool isa(From *value) {
        return dynamic_cast<To *>(value) != nullptr;
    }
}

#endif //X_UTILS_H
