#ifndef X_PIPELINE_H
#define X_PIPELINE_H

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
        // todo pass by ref
        Pipeline &pipe(Pipe *pipe) {
            node = pipe->handle(node);

            return *this;
        }
    };
}

#endif //X_PIPELINE_H
