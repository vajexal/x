#ifndef X_CHECK_INTERFACES_H
#define X_CHECK_INTERFACES_H

#include "pipeline.h"
#include "compiler_runtime.h"

#include <map>

namespace X::Pipes {
    class CheckInterfaces : public Pipe {
        CompilerRuntime &compilerRuntime;

        // class name -> all methods (including parents) {method name -> method def}
        std::map<std::string, std::map<std::string, MethodDefNode *>> classMethods;

    public:
        CheckInterfaces(CompilerRuntime &compilerRuntime) : compilerRuntime(compilerRuntime) {}

        StatementListNode *handle(StatementListNode *node) override;

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

#endif //X_CHECK_INTERFACES_H
