#include "const_string_folding.h"

#include <variant>
#include <string>

namespace X::Pipes {
    TopStatementListNode *ConstStringFolding::handle(TopStatementListNode *node) {
        visitor.visit(node, [](BinaryNode *node) -> Node * {
            if (node->opType != OpType::PLUS) {
                return node;
            }

            auto lhs = llvm::dyn_cast<ScalarNode>(node->lhs);
            auto rhs = llvm::dyn_cast<ScalarNode>(node->rhs);

            if (lhs && rhs && lhs->type.is(Type::TypeID::STRING) && rhs->type.is(Type::TypeID::STRING)) {
                auto val = std::get<std::string>(lhs->value) + std::get<std::string>(rhs->value);
                auto result = new ScalarNode(Type::scalar(Type::TypeID::STRING), std::move(val));
                delete node;
                return result;
            }

            return node;
        });

        return node;
    }
}
