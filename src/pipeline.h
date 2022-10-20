#ifndef X_PIPELINE_H
#define X_PIPELINE_H

#include "ast.h"

namespace X {
    class Pipe {
    public:
        virtual StatementListNode *handle(StatementListNode *node) = 0;
    };

    class Pipeline {
        StatementListNode *node = nullptr;

    public:
        Pipeline &send(StatementListNode *newNode) {
            node = newNode;

            return *this;
        }

        // todo pass by ref
        Pipeline &pipe(Pipe *pipe) {
            node = pipe->handle(node);

            return *this;
        }

        Node *get() {
            return node;
        }
    };
}

#endif //X_PIPELINE_H
