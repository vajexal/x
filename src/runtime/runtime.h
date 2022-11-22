#ifndef X_RUNTIME_H
#define X_RUNTIME_H

#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include "runtime/string.h"
#include "runtime/array.h"
#include "runtime/range.h"
#include "mangler.h"

namespace X::Runtime {
    void println(String *str);

    /// returns true is stings are equal
    bool compareStrings(String *first, String *second);

    class Runtime {
        Mangler mangler;

        std::vector<std::tuple<llvm::Function *, void *>> fnDefinitions;
    public:
        void addDeclarations(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module);
        void addDefinitions(llvm::ExecutionEngine &engine);
    };
}

#endif //X_RUNTIME_H
