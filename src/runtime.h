#ifndef X_RUNTIME_H
#define X_RUNTIME_H

#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

namespace X {
    void _runtime_print(int x);

    class Runtime {
        std::map<std::string, llvm::Function *> functions;

    public:
        void addDeclarations(llvm::LLVMContext &context, llvm::Module &module);
        void addGlobalMappings(llvm::ExecutionEngine &engine);
    };
}

#endif //X_RUNTIME_H
