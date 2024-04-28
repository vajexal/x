#pragma once

#include <unordered_map>

#include "pipeline.h"
#include "ast.h"

namespace X::Pipes {
    class CheckAbstractClasses : public Pipe {
        // class name -> {method name -> abstract method decl}
        std::unordered_map<std::string, std::unordered_map<std::string, MethodDeclNode *>> classAbstractMethods;

    public:
        TopStatementListNode *handle(TopStatementListNode *node) override;

    private:
        void checkClass(ClassNode *node);
    };

    class CheckAbstractClassesException : public std::exception {
        std::string message;

    public:
        CheckAbstractClassesException(const char *m) : message(m) {}
        CheckAbstractClassesException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };
}
