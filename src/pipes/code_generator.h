#pragma once

#include "llvm/Support/Error.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/Core.h"

#include "pipeline.h"
#include "mangler.h"
#include "compiler_runtime.h"

namespace X::Pipes {
    // todo rename
    class CodeGenerator : public Pipe {
        std::shared_ptr<CompilerRuntime> compilerRuntime;
        std::string sourceName;

    public:
        CodeGenerator(std::shared_ptr<CompilerRuntime> compilerRuntime, std::string sourceName) : compilerRuntime(std::move(compilerRuntime)),
                                                                                                  sourceName(std::move(sourceName)) {}

        TopStatementListNode *handle(TopStatementListNode *node) override;

    private:
        void throwOnError(llvm::Error &&err) const;

        template<typename T>
        T throwOnError(llvm::Expected<T> &&val) const;
    };

    class OptimizationTransform {
        std::shared_ptr<Mangler> mangler;

    public:
        OptimizationTransform(std::shared_ptr<Mangler> mangler) : mangler(std::move(mangler)) {}

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
