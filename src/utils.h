#pragma once

#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace X {
    inline const std::string CONSTRUCTOR_FN_NAME = "construct";
    inline const std::string THIS_KEYWORD = "this";
    inline const std::string SELF_KEYWORD = "self";

    llvm::ConstantInt *getTypeSize(llvm::Module &module, llvm::Type *type);
}
