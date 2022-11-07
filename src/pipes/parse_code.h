#ifndef X_PARSE_CODE_H
#define X_PARSE_CODE_H

#include "pipeline.h"

namespace X::Pipes {
    class ParseCode : public Pipe {
        std::string code;

    public:
        ParseCode(const std::string &code) {
            this->code = std::move(code + '\n'); // hack for parser
        }

        StatementListNode *handle(StatementListNode *node) override;
    };

    class ParseCodeException : public std::exception {
        const char *message;

    public:
        ParseCodeException(const char *m) : message(m) {}

        const char *what() const noexcept override {
            return message;
        }
    };
}

#endif //X_PARSE_CODE_H
