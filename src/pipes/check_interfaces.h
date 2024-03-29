#pragma once

#include "pipeline.h"
#include "compiler_runtime.h"

#include <map>

namespace X::Pipes {
    class CheckInterfaces : public Pipe {
        CompilerRuntime &compilerRuntime;

        // class name -> all methods (including parents) {method name -> method def}
        std::map<std::string, std::map<std::string, MethodDefNode *>> classMethods;

        std::set<std::string> abstractClasses;
    public:
        CheckInterfaces(CompilerRuntime &compilerRuntime) : compilerRuntime(compilerRuntime) {}

        TopStatementListNode *handle(TopStatementListNode *node) override;

    private:
        void addInterface(InterfaceNode *node);
        void addMethodToInterface(const std::string &interfaceName, MethodDeclNode *node);
        void checkClass(ClassNode *node);
    };

    class CheckInterfacesException : public std::exception {
        std::string message;

    public:
        CheckInterfacesException(const char *m) : message(m) {}
        CheckInterfacesException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };
}
