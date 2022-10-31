#ifndef X_CHECK_INTERFACES_H
#define X_CHECK_INTERFACES_H

#include "pipeline.h"

#include <map>

namespace X::Pipes {
    class CheckInterfaces : public Pipe {
        // interface name -> {method name -> method decl}
        std::map<std::string, std::map<std::string, MethodDeclNode *>> interfaceMethods;

    public:
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
