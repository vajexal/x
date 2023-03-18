#pragma once

#include "llvm/IR/Type.h"

namespace X {
    inline const std::string CONSTRUCTOR_FN_NAME = "construct";
    inline const std::string THIS_KEYWORD = "this";
    inline const std::string SELF_KEYWORD = "self";

    llvm::Type *deref(llvm::Type *type);
}
