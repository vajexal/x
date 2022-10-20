#ifndef X_CODE_GENERATOR_H
#define X_CODE_GENERATOR_H

#include "pipeline.h"

namespace X::Pipes {
    class CodeGeneratorException : public std::exception {
        const char *message;

    public:
        CodeGeneratorException(const char *m) : message(m) {}
        CodeGeneratorException(const std::string &s) : message(s.c_str()) {}

        const char *what() const noexcept override {
            return message;
        }
    };

    // todo rename
    class CodeGenerator : public Pipe {
    public:
        StatementListNode *handle(StatementListNode *node) override;
    };
}

#endif //X_CODE_GENERATOR_H
