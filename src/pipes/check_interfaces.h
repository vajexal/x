#ifndef X_CHECK_INTERFACES_H
#define X_CHECK_INTERFACES_H

#include "pipeline.h"

#include <map>

namespace X::Pipes {
    class CheckInterfaces : public Pipe {
        std::map<std::string, InterfaceNode *> interfaces;

    public:
        StatementListNode *handle(StatementListNode *node) override;
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
