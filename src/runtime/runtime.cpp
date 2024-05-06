#include "runtime.h"

#include <iostream>
#include <unordered_map>

#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/ExecutionEngine/JITSymbol.h"

#include "gc/gc.h"
#include "casts.h"
#include "utils.h"

namespace X::Runtime {
    void println(String *str) {
        std::cout << str->str << std::endl;
    }

    void die(const char *s) {
        std::cout << s << std::endl;
        std::exit(1);
    }

    /// returns true is stings are equal
    bool compareStrings(String *first, String *second) {
        return first->len == second->len && std::strncmp(first->str, second->str, first->len) == 0;
    }

    void *gc_alloc(GC::GC **gc, std::size_t size) {
        return (*gc)->alloc(size);
    }

    void *gc_realloc(GC::GC **gc, void *ptr, std::size_t newSize) {
        return (*gc)->realloc(ptr, newSize);
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

    void gc_addGlobalRoot(GC::GC **gc, void **root, GC::Metadata *meta) {
        (*gc)->addGlobalRoot(root, meta);
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module) {
        llvm::StructType::create(context, {builder.getPtrTy(), builder.getInt64Ty()}, String::CLASS_NAME);
        llvm::StructType::create(context, {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt64Ty()}, Range::CLASS_NAME);

        // function name, return type, param types
        std::vector<std::tuple<std::string, llvm::Type *, llvm::ArrayRef<llvm::Type *>>> funcs{
                {Mangler::mangleInternalFunction("die"), builder.getVoidTy(), {builder.getPtrTy()}},
                {"exit", builder.getVoidTy(), {builder.getInt64Ty()}},
                {"println", builder.getVoidTy(), {builder.getPtrTy()}},

                // string

                {Mangler::mangleInternalFunction("castBoolToString"), builder.getPtrTy(), {builder.getInt1Ty()}},
                {Mangler::mangleInternalFunction("castIntToString"), builder.getPtrTy(), {builder.getInt64Ty()}},
                {Mangler::mangleInternalFunction("castFloatToString"), builder.getPtrTy(), {builder.getDoubleTy()}},
                {Mangler::mangleInternalFunction("compareStrings"), builder.getInt1Ty(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, CONSTRUCTOR_FN_NAME), builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "concat"), builder.getPtrTy(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "length"), builder.getInt64Ty(), {builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "isEmpty"), builder.getInt1Ty(), {builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "trim"), builder.getPtrTy(), {builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "toLower"), builder.getPtrTy(), {builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "toUpper"), builder.getPtrTy(), {builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "index"), builder.getInt64Ty(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "contains"), builder.getInt1Ty(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "startsWith"), builder.getInt1Ty(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "endsWith"), builder.getInt1Ty(), {builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "substring"), builder.getPtrTy(),
                 {builder.getPtrTy(), builder.getInt64Ty(), builder.getInt64Ty()}},
                {Mangler::mangleInternalFunction("createEmptyString"), builder.getPtrTy(), {}},

                // range
                {Mangler::mangleInternalMethod(Range::CLASS_NAME, "create"), builder.getPtrTy(),
                 {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt64Ty()}},
                {Mangler::mangleInternalMethod(Range::CLASS_NAME, "length"), builder.getInt64Ty(), {builder.getPtrTy()}},
                {Mangler::mangleInternalMethod(Range::CLASS_NAME, "get[]"), builder.getInt64Ty(), {builder.getPtrTy(), builder.getInt64Ty()}},

                // gc
                {Mangler::mangleInternalFunction("gcAlloc"), builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()}},
                {Mangler::mangleInternalFunction("gcRealloc"), builder.getPtrTy(), {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()}},
                {Mangler::mangleInternalFunction("gcPushStackFrame"), builder.getVoidTy(), {builder.getPtrTy()}},
                {Mangler::mangleInternalFunction("gcPopStackFrame"), builder.getVoidTy(), {builder.getPtrTy()}},
                {Mangler::mangleInternalFunction("gcAddRoot"), builder.getPtrTy(), {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()}},
                {Mangler::mangleInternalFunction("gcAddGlobalRoot"), builder.getPtrTy(), {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()}},
        };

        for (auto &[fnName, retType, paramTypes]: funcs) {
            auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
            module.getOrInsertFunction(fnName, fnType);
        }

        auto gc = llvm::cast<llvm::GlobalVariable>(
                module.getOrInsertGlobal(Mangler::mangleInternalSymbol("gc"), builder.getPtrTy()));
        gc->setInitializer(llvm::ConstantPointerNull::get(builder.getPtrTy()));
    }

    void Runtime::addDefinitions(llvm::orc::JITDylib &JD, llvm::orc::MangleAndInterner &llvmMangler) {
        static std::vector<std::tuple<std::string, void *>> funcs{
                {Mangler::mangleInternalFunction("die"), reinterpret_cast<void *>(die)},
                {"exit", reinterpret_cast<void *>(std::exit)},
                {"println", reinterpret_cast<void *>(println)},

                // string
                {Mangler::mangleInternalFunction("castBoolToString"), reinterpret_cast<void *>(castBoolToString)},
                {Mangler::mangleInternalFunction("castIntToString"), reinterpret_cast<void *>(castIntToString)},
                {Mangler::mangleInternalFunction("castFloatToString"), reinterpret_cast<void *>(castFloatToString)},
                {Mangler::mangleInternalFunction("compareStrings"), reinterpret_cast<void *>(compareStrings)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, CONSTRUCTOR_FN_NAME), reinterpret_cast<void *>(String_construct)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "concat"), reinterpret_cast<void *>(String_concat)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "length"), reinterpret_cast<void *>(String_length)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "isEmpty"), reinterpret_cast<void *>(String_isEmpty)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "trim"), reinterpret_cast<void *>(String_trim)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "toLower"), reinterpret_cast<void *>(String_toLower)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "toUpper"), reinterpret_cast<void *>(String_toUpper)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "index"), reinterpret_cast<void *>(String_index)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "contains"), reinterpret_cast<void *>(String_contains)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "startsWith"), reinterpret_cast<void *>(String_startsWith)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "endsWith"), reinterpret_cast<void *>(String_endsWith)},
                {Mangler::mangleInternalMethod(String::CLASS_NAME, "substring"), reinterpret_cast<void *>(String_substring)},
                {Mangler::mangleInternalFunction("createEmptyString"), reinterpret_cast<void *>(createEmptyString)},

                // range
                {Mangler::mangleInternalMethod(Range::CLASS_NAME, "length"), reinterpret_cast<void *>(Range_length)},
                {Mangler::mangleInternalMethod(Range::CLASS_NAME, "get[]"), reinterpret_cast<void *>(Range_get)},

                // gc
                {Mangler::mangleInternalFunction("gcAlloc"), reinterpret_cast<void *>(gc_alloc)},
                {Mangler::mangleInternalFunction("gcRealloc"), reinterpret_cast<void *>(gc_realloc)},
                {Mangler::mangleInternalFunction("gcPushStackFrame"), reinterpret_cast<void *>(gc_pushStackFrame)},
                {Mangler::mangleInternalFunction("gcPopStackFrame"), reinterpret_cast<void *>(gc_popStackFrame)},
                {Mangler::mangleInternalFunction("gcAddRoot"), reinterpret_cast<void *>(gc_addRoot)},
                {Mangler::mangleInternalFunction("gcAddGlobalRoot"), reinterpret_cast<void *>(gc_addGlobalRoot)},
        };

        llvm::StringMap<void *> builtinFuncs;

        for (auto &[name, ptr]: funcs) {
            builtinFuncs[*llvmMangler(name)] = ptr;
        }

        JD.addGenerator(std::make_unique<RuntimeBuiltinGenerator>(std::move(builtinFuncs)));
    }

    llvm::Error RuntimeBuiltinGenerator::tryToGenerate(llvm::orc::LookupState &LS, llvm::orc::LookupKind K, llvm::orc::JITDylib &JD,
                                                       llvm::orc::JITDylibLookupFlags JDLookupFlags, const llvm::orc::SymbolLookupSet &LookupSet) {
        llvm::orc::SymbolMap symbols;

        for (auto &[name, flags]: LookupSet) {
            auto it = builtinFuncs.find(*name);
            if (it != builtinFuncs.end()) {
                symbols.try_emplace(name, llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(it->second), llvm::JITSymbolFlags()));
            }
        }

        if (symbols.empty()) {
            return llvm::Error::success();
        }

        return JD.define(llvm::orc::absoluteSymbols(std::move(symbols)));
    }
}
