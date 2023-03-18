#pragma once

#include "pipeline.h"

namespace X::Pipes {
    class ParseCode : public Pipe {
        std::string code;

    public:
        ParseCode(const std::string &code) {
            this->code = std::move(code + '\n'); // hack for parser
        }

        TopStatementListNode *handle(TopStatementListNode *node) override;
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
