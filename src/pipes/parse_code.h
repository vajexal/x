#ifndef X_PARSE_CODE_H
#define X_PARSE_CODE_H

#include "pipeline.h"

namespace X::Pipes {
    class ParseCodeException : public std::exception {
        const char *message;

    public:
        ParseCodeException(const char *m) : message(m) {}

        const char *what() const noexcept override {
            return message;
        }
    };

    class ParseCode : public Pipe {
        std::string code;

    public:
        ParseCode(std::string code) : code(std::move(code)) {}

        StatementListNode *handle(StatementListNode *node) override;
    };
}

#endif //X_PARSE_CODE_H
