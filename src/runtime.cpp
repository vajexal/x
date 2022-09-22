#include <iostream>
#include <cstring>

#include "runtime.h"

namespace X {
    void String_construct(String *str, const char *s) {
        str->len = std::strlen(s) + 1;
        str->str = new char[str->len];
        str->str[str->len] = 0;
        std::memcpy(str->str, s, str->len);
    }

    void println(String *str) {
        std::cout << str->str << std::endl;
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::Module &module) {
        auto abortFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
        functions["abort"] = llvm::cast<llvm::Function>(module.getOrInsertFunction("abort", abortFnType).getCallee());

        auto stringType = llvm::StructType::create(
                context, {llvm::Type::getInt8PtrTy(context), llvm::Type::getInt64Ty(context)}, String::CLASS_NAME);
        auto stringConstructFnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context), {stringType->getPointerTo(), llvm::Type::getInt8PtrTy(context)}, false);
        functions[String::CONSTRUCTOR_FN_NAME] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(String::CONSTRUCTOR_FN_NAME, stringConstructFnType).getCallee());

        auto printlnFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {stringType->getPointerTo()}, false);
        functions["println"] = llvm::cast<llvm::Function>(module.getOrInsertFunction("println", printlnFnType).getCallee());
    }

    void Runtime::addDefinitions(llvm::ExecutionEngine &engine) {
        engine.addGlobalMapping(functions["abort"], reinterpret_cast<void *>(std::abort));
        engine.addGlobalMapping(functions[String::CONSTRUCTOR_FN_NAME], reinterpret_cast<void *>(String_construct));
        engine.addGlobalMapping(functions["println"], reinterpret_cast<void *>(println));
    }
}
