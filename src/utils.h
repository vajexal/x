#ifndef X_UTILS_H
#define X_UTILS_H

#include "llvm/IR/Type.h"

namespace X {
    class MethodDeclNode;
    class MethodDefNode;

    inline const std::string CONSTRUCTOR_FN_NAME = "construct";
    inline const std::string THIS_KEYWORD = "this";
    inline const std::string SELF_KEYWORD = "self";

    bool compareDeclAndDef(MethodDeclNode *declNode, MethodDefNode *defNode);
    llvm::Type *deref(llvm::Type *type);
}

#endif //X_UTILS_H
