#include <iostream>
#include <cstring>

#include "runtime.h"

namespace X {
    void String_construct(String *that, const char *s) {
        that->len = std::strlen(s);
        that->str = new char[that->len + 1];
        that->str[that->len] = 0;
        std::memcpy(that->str, s, that->len);
    }

    String *String_concat(String *that, String *other) {
        if (!other->len) {
            return that;
        }

        auto res = new String;
        res->len = that->len + other->len;
        res->str = new char[res->len + 1];
        res->str[res->len] = 0;
        std::memcpy(res->str, that->str, that->len);
        std::memcpy(res->str + that->len, other->str, other->len);
        return res;
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
        auto stringConstructFnName = mangler.mangleMethod(String::CLASS_NAME, "construct");
        functions[stringConstructFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringConstructFnName, stringConstructFnType).getCallee());

        auto stringConcatFnType = llvm::FunctionType::get(
                stringType->getPointerTo(), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        auto stringConcatFnName = mangler.mangleMethod(String::CLASS_NAME, "concat");
        functions[stringConcatFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringConcatFnName, stringConcatFnType).getCallee());

        auto printlnFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {stringType->getPointerTo()}, false);
        functions["println"] = llvm::cast<llvm::Function>(module.getOrInsertFunction("println", printlnFnType).getCallee());
    }

    void Runtime::addDefinitions(llvm::ExecutionEngine &engine) {
        engine.addGlobalMapping(functions["abort"], reinterpret_cast<void *>(std::abort));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "construct")], reinterpret_cast<void *>(String_construct));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "concat")], reinterpret_cast<void *>(String_concat));
        engine.addGlobalMapping(functions["println"], reinterpret_cast<void *>(println));
    }
}
