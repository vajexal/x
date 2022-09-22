#ifndef X_RUNTIME_H
#define X_RUNTIME_H

#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

#include "mangler.h"

namespace X {
    class Runtime {
        Mangler mangler;

        std::map<std::string, llvm::Function *> functions;

    public:
        void addDeclarations(llvm::LLVMContext &context, llvm::Module &module);
        void addDefinitions(llvm::ExecutionEngine &engine);
    };

    struct String {
        static inline const std::string CLASS_NAME = "String";
        static inline const std::string CONSTRUCTOR_FN_NAME = "String_construct";

        char *str;
        uint64_t len;
    };
}

#endif //X_RUNTIME_H
