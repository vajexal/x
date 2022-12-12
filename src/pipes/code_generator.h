#ifndef X_CODE_GENERATOR_H
#define X_CODE_GENERATOR_H

#include "llvm/Support/Error.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/Core.h"

#include "pipeline.h"
#include "compiler_runtime.h"

namespace X::Pipes {
    // todo rename
    class CodeGenerator : public Pipe {
        CompilerRuntime &compilerRuntime;

    public:
        CodeGenerator(CompilerRuntime &compilerRuntime) : compilerRuntime(compilerRuntime) {}

        StatementListNode *handle(StatementListNode *node) override;

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

#endif //X_CODE_GENERATOR_H
