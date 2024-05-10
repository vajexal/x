#pragma once

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/Support/Error.h"
#include "llvm/ADT/StringMap.h"

#include "runtime/string.h"
#include "runtime/array.h"
#include "runtime/range.h"

namespace X::Runtime {
    class Runtime {
    public:
        void addDeclarations(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module);
        void addDefinitions(llvm::orc::JITDylib &JD, llvm::orc::MangleAndInterner &llvmMangler);
    };

    class RuntimeBuiltinGenerator : public llvm::orc::DefinitionGenerator {
        llvm::StringMap<void *> builtinFuncs;

    public:
        explicit RuntimeBuiltinGenerator(llvm::StringMap<void *> builtinFuncs) : builtinFuncs(std::move(builtinFuncs)) {}

        llvm::Error tryToGenerate(llvm::orc::LookupState &LS, llvm::orc::LookupKind K, llvm::orc::JITDylib &JD, llvm::orc::JITDylibLookupFlags JDLookupFlags,
                                  const llvm::orc::SymbolLookupSet &LookupSet) override;
    };
}
