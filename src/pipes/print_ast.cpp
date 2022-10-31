#include "print_ast.h"

#include <variant>

namespace X::Pipes {
    void PrintAst::printNode(Node *node, int level) {
        throw PrintAstException("can't print node");
    }

    void PrintAst::printNode(StatementListNode *node, int level) {
        std::cout << "statement list\n";

        for (auto child: node->getChildren()) {
            child->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(ScalarNode *node, int level) {
        std::cout << "scalar";

        std::visit([](auto val) { std::cout << '(' << val << ')' << std::endl; }, node->getValue());
    }

    void PrintAst::printNode(UnaryNode *node, int level) {
        std::cout << node->getType() << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void PrintAst::printNode(BinaryNode *node, int level) {
        std::cout << node->getType() << std::endl;

        node->getLhs()->print(*this, level + 1);
        node->getRhs()->print(*this, level + 1);
    }

    void PrintAst::printNode(DeclareNode *node, int level) {
        std::cout << node->getType() << ' ' << node->getName() << " = " << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void PrintAst::printNode(AssignNode *node, int level) {
        std::cout << node->getName() << " =" << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void PrintAst::printNode(VarNode *node, int level) {
        std::cout << "var " << node->getName() << std::endl;
    }

    void PrintAst::printNode(IfNode *node, int level) {
        std::cout << "if" << std::endl;

        node->getCond()->print(*this, level + 1);
        node->getThenNode()->print(*this, level + 1);
        if (node->getElseNode()) {
            node->getElseNode()->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(WhileNode *node, int level) {
        std::cout << "while" << std::endl;

        node->getCond()->print(*this, level + 1);
        node->getBody()->print(*this, level + 1);
    }

    void PrintAst::printNode(ArgNode *node, int level) {
        std::cout << "arg " << node->getType() << ' ' << node->getName() << std::endl;
    }

    void PrintAst::printNode(FnDeclNode *node, int level) {
        std::cout << "fn " << node->getName() << " -> " << node->getReturnType() << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(FnDefNode *node, int level) {
        std::cout << "fn " << node->getName() << " -> " << node->getReturnType() << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
        node->getBody()->print(*this, level + 1);
    }

    void PrintAst::printNode(FnCallNode *node, int level) {
        std::cout << "fn call " << node->getName() << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(ReturnNode *node, int level) {
        std::cout << "return" << std::endl;

        if (node->getVal()) {
            node->getVal()->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(PrintlnNode *node, int level) {
        std::cout << "println" << std::endl;

        node->getVal()->print(*this, level + 1);
    }

    void PrintAst::printNode(BreakNode *node, int level) {
        std::cout << "break" << std::endl;
    }

    void PrintAst::printNode(ContinueNode *node, int level) {
        std::cout << "continue" << std::endl;
    }

    void PrintAst::printNode(CommentNode *node, int level) {
        std::cout << "//" << node->getComment() << std::endl;
    }

    void PrintAst::printNode(ClassNode *node, int level) {
        if (node->isAbstract()) {
            std::cout << "abstract ";
        }

        std::cout << "class " << node->getName();

        if (node->hasParent()) {
            std::cout << " extends " << node->getParent();
        }

        if (!node->getInterfaces().empty()) {
            std::cout << " implements ";

            const auto &interfaces = node->getInterfaces();
            for (auto it = interfaces.cbegin(); it != interfaces.cend(); it++) {
                if (it != interfaces.cbegin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
        }

        std::cout << std::endl;

        node->getMembers()->print(*this, level + 1);
    }

    void PrintAst::printNode(ClassMembersNode *node, int level) {
        for (auto &prop: node->getProps()) {
            prop->print(*this, level);
        }

        for (auto &method: node->getMethods()) {
            method->print(*this, level);
        }

        for (auto &method: node->getAbstractMethods()) {
            std::cout << "abstract ";
            method->print(*this, level);
        }
    }

    void PrintAst::printNode(PropDeclNode *node, int level) {
        std::cout << node->getAccessModifier() << ' ';

        if (node->getIsStatic()) {
            std::cout << "static ";
        }

        std::cout << node->getType() << ' ' << node->getName() << std::endl;
    }

    void PrintAst::printNode(MethodDefNode *node, int level) {
        std::cout << node->getAccessModifier() << ' ';

        if (node->getIsStatic()) {
            std::cout << "static ";
        }

        node->getFnDef()->print(*this, level);
    }

    void PrintAst::printNode(FetchPropNode *node, int level) {
        std::cout << '.' << node->getName() << std::endl;

        node->getObj()->print(*this, level + 1);
    }

    void PrintAst::printNode(FetchStaticPropNode *node, int level) {
        std::cout << node->getClassName() << "::" << node->getPropName() << std::endl;
    }

    void PrintAst::printNode(MethodCallNode *node, int level) {
        std::cout << '.' << node->getName() << "()" << std::endl;

        node->getObj()->print(*this, level + 1);

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(StaticMethodCallNode *node, int level) {
        std::cout << node->getClassName() << "::" << node->getMethodName() << "()" << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(AssignPropNode *node, int level) {
        std::cout << '.' << node->getName() << " =" << std::endl;

        node->getObj()->print(*this, level + 1);
        node->getExpr()->print(*this, level + 1);
    }

    void PrintAst::printNode(AssignStaticPropNode *node, int level) {
        std::cout << node->getClassName() << "::" << node->getPropName() << " =" << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void PrintAst::printNode(NewNode *node, int level) {
        std::cout << "new " << node->getName() << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void PrintAst::printNode(MethodDeclNode *node, int level) {
        std::cout << node->getAccessModifier() << ' ';

        if (node->getIsStatic()) {
            std::cout << "static ";
        }

        node->getFnDecl()->print(*this, level);
    }

    void PrintAst::printNode(InterfaceNode *node, int level) {
        std::cout << "interface " << node->getName();

        if (node->hasParents()) {
            std::cout << " extends ";

            auto &interfaces = node->getParents();
            for (auto it = interfaces.cbegin(); it != interfaces.cend(); it++) {
                if (it != interfaces.cbegin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
        }

        std::cout << std::endl;

        for (auto &method: node->getMethods()) {
            method->print(*this, level);
        }
    }
}
