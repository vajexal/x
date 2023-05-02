#pragma once

#include "llvm/Support/Error.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/Core.h"

#include "pipeline.h"
#include "mangler.h"
#include "compiler_runtime.h"

namespace X::Pipes {
    // todo rename
    class CodeGenerator : public Pipe {
        Mangler mangler;
        CompilerRuntime &compilerRuntime;
        std::string sourceName;

    public:
        CodeGenerator(CompilerRuntime &compilerRuntime, std::string sourceName) : compilerRuntime(compilerRuntime), sourceName(std::move(sourceName)) {}

        TopStatementListNode *handle(TopStatementListNode *node) override;

    private:
        void throwOnError(llvm::Error &&err) const;

        template<typename T>
        T throwOnError(llvm::Expected<T> &&val) const;
    };

    class OptimizationTransform {
        static const int OPT_LEVEL = 2;
        static const int SIZE_OPT_LEVEL = 0;

        std::unique_ptr<llvm::legacy::PassManager> PM;

    public:
        OptimizationTransform();

        llvm::Expected<llvm::orc::ThreadSafeModule> operator()(llvm::orc::ThreadSafeModule TSM, llvm::orc::MaterializationResponsibility &R);
    };

    class CodeGeneratorException : public std::exception {
        std::string message;

    public:
        CodeGeneratorException(const char *m) : message(m) {}
        CodeGeneratorException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };
}
