#pragma once

#include "ast.h"

namespace X {
    class Pipe {
    public:
        virtual TopStatementListNode *handle(TopStatementListNode *node) = 0;
    };

    class Pipeline {
        TopStatementListNode *node = nullptr;

    public:
        ~Pipeline() {
            delete node;
        }

    public:
        Pipeline &pipe(Pipe &&pipe) {
            node = pipe.handle(node);

            return *this;
        }
    };
}
