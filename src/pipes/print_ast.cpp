#include "print_ast.h"

#include <variant>

namespace X::Pipes {
    void PrintAst::printNode(Node *node, int level) {
        throw PrintAstException("can't print node");
    }

    void PrintAst::printNode(StatementListNode *node, int level) {
        std::cout << "statement list\n";

        for (auto child: node->children) {
            child->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(ScalarNode *node, int level) {
        std::cout << "scalar(";
        auto value = node->value;

        switch (node->type.getTypeID()) {
            case Type::TypeID::INT:
                std::cout << std::get<int64_t>(value);
                break;
            case Type::TypeID::FLOAT:
                std::cout << std::get<double>(value);
                break;
            case Type::TypeID::BOOL:
                std::cout << std::get<bool>(value);
                break;
            case Type::TypeID::STRING:
                std::cout << std::get<std::string>(value);
                break;
            case Type::TypeID::ARRAY:
                std::cout << '[' << std::endl;
                for (auto &expr: std::get<ExprList>(value)) {
                    expr->print(*this, level + 1);
                }
                std::cout << std::string(level * 2, ' ') << ']';
                break;
            default:
                throw PrintAstException("invalid scalar type");
        }

        std::cout << ')' << std::endl;
    }

    void PrintAst::printNode(UnaryNode *node, int level) {
        std::cout << node->opType << std::endl;

        node->expr->print(*this, level + 1);
    }

    void PrintAst::printNode(BinaryNode *node, int level) {
        std::cout << node->opType << std::endl;

        node->lhs->print(*this, level + 1);
        node->rhs->print(*this, level + 1);
    }

    void PrintAst::printNode(DeclNode *node, int level) {
        std::cout << node->type << ' ' << node->name << " = " << std::endl;

        if (node->expr) {
            node->expr->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(AssignNode *node, int level) {
        std::cout << node->name << " =" << std::endl;

        node->expr->print(*this, level + 1);
    }

    void PrintAst::printNode(VarNode *node, int level) {
        std::cout << "var " << node->name << std::endl;
    }

    void PrintAst::printNode(IfNode *node, int level) {
        std::cout << "if" << std::endl;

        node->cond->print(*this, level + 1);
        node->thenNode->print(*this, level + 1);
        if (node->elseNode) {
            node->elseNode->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(WhileNode *node, int level) {
        std::cout << "while" << std::endl;

        node->cond->print(*this, level + 1);
        node->body->print(*this, level + 1);
    }

    void PrintAst::printNode(ForNode *node, int level) {
        std::cout << "for ";

        if (node->idx) {
            std::cout << node->idx.value() << ", ";
        }

        std::cout << node->val << " in" << std::endl;

        node->expr->print(*this, level + 1);
        node->body->print(*this, level + 1);
    }

    void PrintAst::printNode(RangeNode *node, int level) {
        std::cout << "range";

        if (node->start) {
            node->start->print(*this, level + 1);
        }
        node->stop->print(*this, level + 1);
        if (node->step) {
            node->step->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(BreakNode *node, int level) {
        std::cout << "break" << std::endl;
    }

    void PrintAst::printNode(ContinueNode *node, int level) {
        std::cout << "continue" << std::endl;
    }

    void PrintAst::printNode(ArgNode *node, int level) {
        std::cout << "arg " << node->type << ' ' << node->name << std::endl;
    }

    void PrintAst::printNode(FnDeclNode *node, int level) {
        std::cout << "fn " << node->name << " -> " << node->returnType << std::endl;

        for (auto &arg: node->args) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(FnDefNode *node, int level) {
        node->decl->print(*this);
        node->body->print(*this, level + 1);
    }

    void PrintAst::printNode(FnCallNode *node, int level) {
        std::cout << "fn call " << node->name << std::endl;

        for (auto &arg: node->args) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(ReturnNode *node, int level) {
        std::cout << "return" << std::endl;

        if (node->val) {
            node->val->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(PrintlnNode *node, int level) {
        std::cout << "println" << std::endl;

        node->val->print(*this, level + 1);
    }

    void PrintAst::printNode(CommentNode *node, int level) {
        std::cout << "//" << node->comment << std::endl;
    }

    void PrintAst::printNode(ClassNode *node, int level) {
        if (node->abstract) {
            std::cout << "abstract ";
        }

        std::cout << "class " << node->name;

        if (node->hasParent()) {
            std::cout << " extends " << node->parent;
        }

        if (!node->interfaces.empty()) {
            std::cout << " implements ";

            const auto &interfaces = node->interfaces;
            for (auto it = interfaces.cbegin(); it != interfaces.cend(); it++) {
                if (it != interfaces.cbegin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
        }

        std::cout << std::endl;

        node->body->print(*this, level + 1);
    }

    void PrintAst::printNode(PropDeclNode *node, int level) {
        std::cout << node->accessModifier << ' ';

        if (node->isStatic) {
            std::cout << "static ";
        }

        node->decl->print(*this, level);
    }

    void PrintAst::printNode(MethodDefNode *node, int level) {
        std::cout << node->accessModifier << ' ';

        if (node->isStatic) {
            std::cout << "static ";
        }

        node->fnDef->print(*this, level);
    }

    void PrintAst::printNode(FetchPropNode *node, int level) {
        std::cout << '.' << node->name << std::endl;

        node->obj->print(*this, level + 1);
    }

    void PrintAst::printNode(FetchStaticPropNode *node, int level) {
        std::cout << node->className << "::" << node->propName << std::endl;
    }

    void PrintAst::printNode(MethodCallNode *node, int level) {
        std::cout << '.' << node->name << "()" << std::endl;

        node->obj->print(*this, level + 1);

        for (auto &arg: node->args) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(StaticMethodCallNode *node, int level) {
        std::cout << node->className << "::" << node->methodName << "()" << std::endl;

        for (auto &arg: node->args) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(AssignPropNode *node, int level) {
        std::cout << '.' << node->name << " =" << std::endl;

        node->obj->print(*this, level + 1);
        node->expr->print(*this, level + 1);
    }

    void PrintAst::printNode(AssignStaticPropNode *node, int level) {
        std::cout << node->className << "::" << node->propName << " =" << std::endl;

        node->expr->print(*this, level + 1);
    }

    void PrintAst::printNode(NewNode *node, int level) {
        std::cout << "new " << node->name << std::endl;

        for (auto &arg: node->args) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(MethodDeclNode *node, int level) {
        if (node->isAbstract) {
            std::cout << "abstract ";
        }

        std::cout << node->accessModifier << ' ';

        if (node->isStatic) {
            std::cout << "static ";
        }

        node->fnDecl->print(*this, level);
    }

    void PrintAst::printNode(InterfaceNode *node, int level) {
        std::cout << "interface " << node->name;

        if (node->hasParents()) {
            std::cout << " extends ";

            auto &interfaces = node->parents;
            for (auto it = interfaces.cbegin(); it != interfaces.cend(); it++) {
                if (it != interfaces.cbegin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
        }

        std::cout << std::endl;

        node->body->print(*this, level);
    }

    void PrintAst::printNode(FetchArrNode *node, int level) {
        std::cout << "[]" << std::endl;

        node->arr->print(*this, level + 1);
        node->idx->print(*this, level + 1);
    }

    void PrintAst::printNode(AssignArrNode *node, int level) {
        std::cout << "[] = " << std::endl;

        node->arr->print(*this, level + 1);
        node->idx->print(*this, level + 1);
        node->expr->print(*this, level + 1);
    }

    void PrintAst::printNode(AppendArrNode *node, int level) {
        std::cout << "[] = " << std::endl;

        node->arr->print(*this, level + 1);
        node->expr->print(*this, level + 1);
    }
}
