#ifndef X_CODE_GENERATOR_H
#define X_CODE_GENERATOR_H

#include "pipeline.h"

namespace X::Pipes {
    // todo rename
    class CodeGenerator : public Pipe {
    public:
        StatementListNode *handle(StatementListNode *node) override;
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