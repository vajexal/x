#include <iostream>

#include "runtime.h"

namespace X {
    void _runtime_print(uint64_t x) {
        std::cout << x << std::endl;
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::Module &module) {
        auto printFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {llvm::Type::getInt64Ty(context)}, false);
        functions["print"] = llvm::dyn_cast<llvm::Function>(module.getOrInsertFunction("print", printFnType).getCallee());

        auto abortFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
        functions["abort"] = llvm::dyn_cast<llvm::Function>(module.getOrInsertFunction("abort", abortFnType).getCallee());
    }

    void Runtime::addDefinitions(llvm::ExecutionEngine &engine) {
        engine.addGlobalMapping(functions["print"], reinterpret_cast<void *>(_runtime_print));
        engine.addGlobalMapping(functions["abort"], reinterpret_cast<void *>(std::abort));
    }
}
