#pragma once

#include "pipeline.h"
#include "visitor.h"

namespace X::Pipes {
    class ConstStringFolding : public Pipe {
        Visitor<BinaryNode> visitor;

    public:
        TopStatementListNode *handle(TopStatementListNode *node) override;
    };
}
