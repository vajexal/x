#pragma once

#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/Support/Error.h"

#include "runtime/string.h"
#include "runtime/array.h"
#include "runtime/range.h"
#include "mangler.h"

namespace X::Runtime {
    void println(String *str);

    /// returns true is stings are equal
    bool compareStrings(String *first, String *second);

    class Runtime {
        Mangler mangler;

    public:
        void addDeclarations(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module);
        void addDefinitions(llvm::orc::JITDylib &JD, llvm::orc::MangleAndInterner &llvmMangle);
    };

    class RuntimeBuiltinGenerator : public llvm::orc::DefinitionGenerator {
        Mangler mangler;

        llvm::orc::SymbolMap builtinFuncs;

    public:
        explicit RuntimeBuiltinGenerator(llvm::orc::MangleAndInterner &llvmMangle);

        llvm::Error tryToGenerate(llvm::orc::LookupState &LS, llvm::orc::LookupKind K, llvm::orc::JITDylib &JD, llvm::orc::JITDylibLookupFlags JDLookupFlags,
                                  const llvm::orc::SymbolLookupSet &LookupSet) override;
    };
}
