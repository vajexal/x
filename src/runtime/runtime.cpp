#include "runtime.h"

#include <iostream>

#include "casts.h"

namespace X::Runtime {
    void println(String *str) {
        std::cout << str->str << std::endl;
    }

    bool compareStrings(String *first, String *second) {
        return first->len == second->len && std::strncmp(first->str, second->str, first->len) == 0;
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::Module &module) {
        auto stringType = llvm::StructType::create(
                context, {llvm::Type::getInt8PtrTy(context), llvm::Type::getInt64Ty(context)}, String::CLASS_NAME);

        // function name, return type, param types, function pointer
        static std::vector<std::tuple<std::string, llvm::Type *, llvm::ArrayRef<llvm::Type *>, void *>> functions{
                {"abort", llvm::Type::getVoidTy(context), {}, reinterpret_cast<void *>(std::abort)},
                {"println", llvm::Type::getVoidTy(context), {stringType->getPointerTo()}, reinterpret_cast<void *>(println)},
                {".castBoolToString", stringType->getPointerTo(), {llvm::Type::getInt1Ty(context)}, reinterpret_cast<void *>(castBoolToString)},
                {".castIntToString", stringType->getPointerTo(), {llvm::Type::getInt64Ty(context)}, reinterpret_cast<void *>(castIntToString)},
                {".castFloatToString", stringType->getPointerTo(), {llvm::Type::getFloatTy(context)}, reinterpret_cast<void *>(castFloatToString)},
                {".compareStrings",
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, reinterpret_cast<void *>(compareStrings)},
                {mangler.mangleMethod(String::CLASS_NAME, "construct"),
                 llvm::Type::getVoidTy(context), {stringType->getPointerTo(), llvm::Type::getInt8PtrTy(context)}, reinterpret_cast<void *>(String_construct)},
                {mangler.mangleMethod(String::CLASS_NAME, "concat"),
                 stringType->getPointerTo(), {stringType->getPointerTo(), stringType->getPointerTo()}, reinterpret_cast<void *>(String_concat)},
                {mangler.mangleMethod(String::CLASS_NAME, "length"),
                 llvm::Type::getInt64Ty(context), {stringType->getPointerTo()}, reinterpret_cast<void *>(String_length)},
                {mangler.mangleMethod(String::CLASS_NAME, "isEmpty"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo()}, reinterpret_cast<void *>(String_isEmpty)},
                {mangler.mangleMethod(String::CLASS_NAME, "trim"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}, reinterpret_cast<void *>(String_trim)},
                {mangler.mangleMethod(String::CLASS_NAME, "toLower"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}, reinterpret_cast<void *>(String_toLower)},
                {mangler.mangleMethod(String::CLASS_NAME, "toUpper"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}, reinterpret_cast<void *>(String_toUpper)},
                {mangler.mangleMethod(String::CLASS_NAME, "index"),
                 llvm::Type::getInt64Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, reinterpret_cast<void *>(String_index)},
                {mangler.mangleMethod(String::CLASS_NAME, "contains"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, reinterpret_cast<void *>(String_contains)},
                {mangler.mangleMethod(String::CLASS_NAME, "startsWith"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, reinterpret_cast<void *>(String_startsWith)},
                {mangler.mangleMethod(String::CLASS_NAME, "endsWith"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, reinterpret_cast<void *>(String_endsWith)},
                {mangler.mangleMethod(String::CLASS_NAME, "substring"),
                 stringType->getPointerTo(), {stringType->getPointerTo(), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)},
                 reinterpret_cast<void *>(String_substring)},
        };

        fnDefinitions.reserve(functions.size());

        for (auto [fnName, retType, paramTypes, fnPtr]: functions) {
            auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
            auto fn = module.getOrInsertFunction(fnName, fnType).getCallee();
            fnDefinitions.emplace_back(llvm::cast<llvm::Function>(fn), fnPtr);
        }
    }

    void Runtime::addDefinitions(llvm::ExecutionEngine &engine) {
        for (auto [fn, fnPtr]: fnDefinitions) {
            engine.addGlobalMapping(fn, fnPtr);
        }
    }
}
