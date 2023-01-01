#ifndef X_CHECK_VIRTUAL_METHODS_H
#define X_CHECK_VIRTUAL_METHODS_H

#include "pipeline.h"

#include <map>
#include <string>

#include "compiler_runtime.h"

namespace X::Pipes {
    class CheckVirtualMethods : public Pipe {
        CompilerRuntime &compilerRuntime;
        std::map<std::string, ClassNode *> classes;

    public:
        CheckVirtualMethods(CompilerRuntime &compilerRuntime) : compilerRuntime(compilerRuntime) {}

        StatementListNode *handle(StatementListNode *node) override;

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

#endif //X_CHECK_VIRTUAL_METHODS_H
