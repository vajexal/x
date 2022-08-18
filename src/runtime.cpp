#include <iostream>

#include "runtime.h"

namespace X {
    void _runtime_print(int x) {
        std::cout << x << std::endl;
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::Module &module) {
        auto printFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {llvm::Type::getInt64Ty(context)}, false);
        functions["print"] = llvm::dyn_cast<llvm::Function>(module.getOrInsertFunction("print", printFnType).getCallee());
    }

    void Runtime::addGlobalMappings(llvm::ExecutionEngine &engine) {
        engine.addGlobalMapping(functions["print"], reinterpret_cast<void *>(_runtime_print));
    }
}
