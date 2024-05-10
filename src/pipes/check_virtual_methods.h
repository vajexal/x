#pragma once

#include "pipeline.h"

#include <unordered_map>
#include <string>

#include "compiler_runtime.h"

namespace X::Pipes {
    class CheckVirtualMethods : public Pipe {
        std::shared_ptr<CompilerRuntime> compilerRuntime;

        std::unordered_map<std::string, ClassNode *> classes;

    public:
        CheckVirtualMethods(std::shared_ptr<CompilerRuntime> compilerRuntime) : compilerRuntime(std::move(compilerRuntime)) {}

        TopStatementListNode *handle(TopStatementListNode *node) override;

    private:
        void checkClass(ClassNode *node);
        ClassNode *getClass(const std::string &className) const;
    };

    class CheckVirtualMethodsException : public std::exception {
        std::string message;

    public:
        CheckVirtualMethodsException(const char *m) : message(m) {}
        CheckVirtualMethodsException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };
}
