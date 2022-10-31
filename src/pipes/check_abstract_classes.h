#ifndef X_CHECK_ABSTRACT_CLASSES_H
#define X_CHECK_ABSTRACT_CLASSES_H

#include <map>

#include "pipeline.h"
#include "ast.h"

namespace X::Pipes {
    class CheckAbstractClasses : public Pipe {
        // class name -> {method name -> abstract method decl}
        std::map<std::string, std::map<std::string, MethodDeclNode *>> classAbstractMethods;

    public:
        StatementListNode *handle(StatementListNode *node) override;

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

#endif //X_CHECK_ABSTRACT_CLASSES_H
