#include "runtime.h"

#include <iostream>

#include "llvm/IR/GlobalVariable.h"
#include "llvm/ExecutionEngine/JITSymbol.h"

#include "casts.h"
#include "utils.h"

namespace X::Runtime {
    void println(String *str) {
        std::cout << str->str << std::endl;
    }

    /// returns true is stings are equal
    bool compareStrings(String *first, String *second) {
        return first->len == second->len && std::strncmp(first->str, second->str, first->len) == 0;
    }

    void *gc_alloc(GC::GC **gc, std::size_t size) {
        return (*gc)->alloc(size);
    }

    void gc_pushStackFrame(GC::GC **gc) {
        (*gc)->pushStackFrame();
    }

    void gc_popStackFrame(GC::GC **gc) {
        (*gc)->popStackFrame();
    }

    void gc_addRoot(GC::GC **gc, void **root, GC::Metadata *meta) {
        (*gc)->addRoot(root, meta);
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module) {
        auto stringType = llvm::StructType::create(
                context, {llvm::Type::getInt8PtrTy(context), llvm::Type::getInt64Ty(context)}, String::CLASS_NAME);

        auto rangeType = llvm::StructType::create(
                context, {llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}, Range::CLASS_NAME);

        auto gcType = llvm::Type::getInt8PtrTy(context)->getPointerTo();

        // function name, return type, param types
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
                {mangler.mangleInternalMethod(String::CLASS_NAME, CONSTRUCTOR_FN_NAME),
                 llvm::Type::getVoidTy(context), {stringType->getPointerTo(), llvm::Type::getInt8PtrTy(context)}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "concat"),
                 stringType->getPointerTo(), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "length"),
                 llvm::Type::getInt64Ty(context), {stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "isEmpty"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "trim"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "toLower"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "toUpper"),
                 stringType->getPointerTo(), {stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "index"),
                 llvm::Type::getInt64Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "contains"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "startsWith"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "endsWith"),
                 llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "substring"),
                 stringType->getPointerTo(), {stringType->getPointerTo(), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}},
                {mangler.mangleInternalFunction("createEmptyString"), stringType->getPointerTo(), {}},

                // range
                {mangler.mangleInternalMethod(Range::CLASS_NAME, "create"),
                 rangeType->getPointerTo(), {llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}},
                {mangler.mangleInternalMethod(Range::CLASS_NAME, "length"),
                 llvm::Type::getInt64Ty(context), {rangeType->getPointerTo()}},
                {mangler.mangleInternalMethod(Range::CLASS_NAME, "get[]"),
                 llvm::Type::getInt64Ty(context), {rangeType->getPointerTo(), llvm::Type::getInt64Ty(context)}},

                // gc
                {mangler.mangleInternalFunction("gcAlloc"), llvm::Type::getInt8PtrTy(context), {gcType, llvm::Type::getInt64Ty(context)}},
                {mangler.mangleInternalFunction("gcPushStackFrame"), llvm::Type::getVoidTy(context), {gcType}},
                {mangler.mangleInternalFunction("gcPopStackFrame"), llvm::Type::getVoidTy(context), {gcType}},
                {mangler.mangleInternalFunction("gcAddRoot"), llvm::Type::getInt8PtrTy(context),
                 {gcType, llvm::Type::getInt8PtrTy(context)->getPointerTo(), llvm::Type::getInt8PtrTy(context)}},
        };

        for (auto &[fnName, retType, paramTypes]: funcs) {
            auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
            module.getOrInsertFunction(fnName, fnType).getCallee();
        }

        auto gc = llvm::cast<llvm::GlobalVariable>(
                module.getOrInsertGlobal(mangler.mangleInternalSymbol("gc"), llvm::Type::getInt8PtrTy(context)));
        gc->setInitializer(llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(context)));
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
                {mangler.mangleInternalMethod(String::CLASS_NAME, CONSTRUCTOR_FN_NAME), reinterpret_cast<void *>(String_construct)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "concat"), reinterpret_cast<void *>(String_concat)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "length"), reinterpret_cast<void *>(String_length)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "isEmpty"), reinterpret_cast<void *>(String_isEmpty)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "trim"), reinterpret_cast<void *>(String_trim)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "toLower"), reinterpret_cast<void *>(String_toLower)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "toUpper"), reinterpret_cast<void *>(String_toUpper)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "index"), reinterpret_cast<void *>(String_index)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "contains"), reinterpret_cast<void *>(String_contains)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "startsWith"), reinterpret_cast<void *>(String_startsWith)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "endsWith"), reinterpret_cast<void *>(String_endsWith)},
                {mangler.mangleInternalMethod(String::CLASS_NAME, "substring"), reinterpret_cast<void *>(String_substring)},
                {mangler.mangleInternalFunction("createEmptyString"), reinterpret_cast<void *>(createEmptyString)},

                // range
                {mangler.mangleInternalMethod(Range::CLASS_NAME, "create"), reinterpret_cast<void *>(Range_create)},
                {mangler.mangleInternalMethod(Range::CLASS_NAME, "length"), reinterpret_cast<void *>(Range_length)},
                {mangler.mangleInternalMethod(Range::CLASS_NAME, "get[]"), reinterpret_cast<void *>(Range_get)},

                // gc
                {mangler.mangleInternalFunction("gcAlloc"), reinterpret_cast<void *>(gc_alloc)},
                {mangler.mangleInternalFunction("gcPushStackFrame"), reinterpret_cast<void *>(gc_pushStackFrame)},
                {mangler.mangleInternalFunction("gcPopStackFrame"), reinterpret_cast<void *>(gc_popStackFrame)},
                {mangler.mangleInternalFunction("gcAddRoot"), reinterpret_cast<void *>(gc_addRoot)},
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
