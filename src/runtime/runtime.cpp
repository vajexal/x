#include "runtime.h"

#include <iostream>

#include "llvm/ExecutionEngine/JITSymbol.h"

#include "casts.h"

namespace X::Runtime {
    void println(String *str) {
        std::cout << str->str << std::endl;
    }

    bool compareStrings(String *first, String *second) {
        return first->len == second->len && std::strncmp(first->str, second->str, first->len) == 0;
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module) {
        auto stringType = llvm::StructType::create(
                context, {llvm::Type::getInt8PtrTy(context), llvm::Type::getInt64Ty(context)}, String::CLASS_NAME);

        auto rangeType = llvm::StructType::create(
                context, {llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}, Range::CLASS_NAME);

        // function name, return type, param types, function pointer
        std::vector<std::tuple<std::string, llvm::Type *, llvm::ArrayRef<llvm::Type *>>> funcs{
                {mangler.mangleInternalFunction("malloc"),
                 llvm::Type::getInt8PtrTy(context), {llvm::Type::getInt64Ty(context)}},
                {mangler.mangleInternalFunction("realloc"), llvm::Type::getInt8PtrTy(context),
                 {llvm::Type::getInt8PtrTy(context), llvm::Type::getInt64Ty(context)}},
                {"exit", llvm::Type::getVoidTy(context), {llvm::Type::getInt64Ty(context)}},
                {"println", llvm::Type::getVoidTy(context), {stringType->getPointerTo()}},

                // string

                {mangler.mangleInternalFunction("castBoolToString"), stringType->getPointerTo(), {llvm::Type::getInt1Ty(context)}},
                {mangler.mangleInternalFunction("castIntToString"), stringType->getPointerTo(), {llvm::Type::getInt64Ty(context)}},
                {mangler.mangleInternalFunction("castFloatToString"), stringType->getPointerTo(), {llvm::Type::getDoubleTy(context)}},
                {mangler.mangleInternalFunction("compareStrings"), llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "construct"),
                 llvm::Type::getVoidTy(context), {stringType->getPointerTo(), llvm::Type::getInt8PtrTy(context)}},
                {mangler.mangleMethod(String::CLASS_NAME, "concat"),
                 stringType->getPointerTo(), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "length"),
                 llvm::Type::getInt64Ty(context), {stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "isEmpty"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "trim"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "toLower"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "toUpper"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "index"),
                 llvm::Type::getInt64Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "contains"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "startsWith"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "endsWith"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleMethod(String::CLASS_NAME, "substring"),
                 stringType->getPointerTo(), {stringType->getPointerTo(), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}},
                {mangler.mangleInternalFunction("createEmptyString"), stringType->getPointerTo(), {}},

                // range
                {mangler.mangleMethod(Range::CLASS_NAME, "create"),
                 rangeType->getPointerTo(), {llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}},
                {mangler.mangleMethod(Range::CLASS_NAME, "length"),
                 llvm::Type::getInt64Ty(context), {rangeType->getPointerTo()}},
                {mangler.mangleMethod(Range::CLASS_NAME, "get[]"),
                 llvm::Type::getInt64Ty(context), {rangeType->getPointerTo(), llvm::Type::getInt64Ty(context)}},
        };

        for (auto &[fnName, retType, paramTypes]: funcs) {
            auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
            module.getOrInsertFunction(fnName, fnType).getCallee();
        }
    }

    void Runtime::addDefinitions(llvm::orc::JITDylib &JD, llvm::orc::MangleAndInterner &llvmMangle) {
        JD.addGenerator(std::make_unique<RuntimeBuiltinGenerator>(llvmMangle));
    }

    RuntimeBuiltinGenerator::RuntimeBuiltinGenerator(llvm::orc::MangleAndInterner &llvmMangle) {
        static std::vector<std::tuple<std::string, void *>> funcs{
                {mangler.mangleInternalFunction("malloc"), reinterpret_cast<void *>(std::malloc)},
                {mangler.mangleInternalFunction("realloc"), reinterpret_cast<void *>(std::realloc)},
                {"exit", reinterpret_cast<void *>(std::exit)},
                {"println", reinterpret_cast<void *>(println)},

                // string
                {mangler.mangleInternalFunction("castBoolToString"), reinterpret_cast<void *>(castBoolToString)},
                {mangler.mangleInternalFunction("castIntToString"), reinterpret_cast<void *>(castIntToString)},
                {mangler.mangleInternalFunction("castFloatToString"), reinterpret_cast<void *>(castFloatToString)},
                {mangler.mangleInternalFunction("compareStrings"), reinterpret_cast<void *>(compareStrings)},
                {mangler.mangleMethod(String::CLASS_NAME, "construct"), reinterpret_cast<void *>(String_construct)},
                {mangler.mangleMethod(String::CLASS_NAME, "concat"), reinterpret_cast<void *>(String_concat)},
                {mangler.mangleMethod(String::CLASS_NAME, "length"), reinterpret_cast<void *>(String_length)},
                {mangler.mangleMethod(String::CLASS_NAME, "isEmpty"), reinterpret_cast<void *>(String_isEmpty)},
                {mangler.mangleMethod(String::CLASS_NAME, "trim"), reinterpret_cast<void *>(String_trim)},
                {mangler.mangleMethod(String::CLASS_NAME, "toLower"), reinterpret_cast<void *>(String_toLower)},
                {mangler.mangleMethod(String::CLASS_NAME, "toUpper"), reinterpret_cast<void *>(String_toUpper)},
                {mangler.mangleMethod(String::CLASS_NAME, "index"), reinterpret_cast<void *>(String_index)},
                {mangler.mangleMethod(String::CLASS_NAME, "contains"), reinterpret_cast<void *>(String_contains)},
                {mangler.mangleMethod(String::CLASS_NAME, "startsWith"), reinterpret_cast<void *>(String_startsWith)},
                {mangler.mangleMethod(String::CLASS_NAME, "endsWith"), reinterpret_cast<void *>(String_endsWith)},
                {mangler.mangleMethod(String::CLASS_NAME, "substring"), reinterpret_cast<void *>(String_substring)},
                {mangler.mangleInternalFunction("createEmptyString"), reinterpret_cast<void *>(createEmptyString)},

                // range
                {mangler.mangleMethod(Range::CLASS_NAME, "create"), reinterpret_cast<void *>(Range_create)},
                {mangler.mangleMethod(Range::CLASS_NAME, "length"), reinterpret_cast<void *>(Range_length)},
                {mangler.mangleMethod(Range::CLASS_NAME, "get[]"), reinterpret_cast<void *>(Range_get)},
        };

        builtinFuncs.init(funcs.size());

        for (auto &[fnName, fnPtr]: funcs) {
            builtinFuncs[llvmMangle(fnName)] = llvm::JITEvaluatedSymbol(llvm::pointerToJITTargetAddress(fnPtr), llvm::JITSymbolFlags());
        }
    }

    llvm::Error RuntimeBuiltinGenerator::tryToGenerate(llvm::orc::LookupState &LS, llvm::orc::LookupKind K, llvm::orc::JITDylib &JD,
                                                       llvm::orc::JITDylibLookupFlags JDLookupFlags, const llvm::orc::SymbolLookupSet &LookupSet) {
        llvm::orc::SymbolMap symbols;

        for (auto &[name, flags]: LookupSet) {
            auto it = builtinFuncs.find(name);
            if (it != builtinFuncs.end()) {
                symbols.insert(*it);
            }
        }

        if (symbols.empty()) {
            return llvm::Error::success();
        }

        return JD.define(llvm::orc::absoluteSymbols(std::move(symbols)));
    }
}
