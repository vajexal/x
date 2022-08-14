#include <variant>

#include "ast_printer.h"

namespace X {
    void AstPrinter::printNode(Node *node, int level) {
        throw AstPrinterException("can't print node");
    }

    void AstPrinter::printNode(StatementListNode *node, int level) {
        std::cout << "statement list\n";

        for (auto child: node->getChildren()) {
            child->print(*this, level + 1);
        }
    }

    void AstPrinter::printNode(ScalarNode *node, int level) {
        std::cout << "scalar";

        std::visit([](auto val) { std::cout << '(' << val << ')' << std::endl; }, node->getValue());
    }

    void AstPrinter::printNode(UnaryNode *node, int level) {
        std::cout << node->getType() << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void AstPrinter::printNode(BinaryNode *node, int level) {
        std::cout << node->getType() << std::endl;

        node->getLhs()->print(*this, level + 1);
        node->getRhs()->print(*this, level + 1);
    }

    void AstPrinter::printNode(DeclareNode *node, int level) {
        std::cout << "declare " << node->getType() << " " << node->getName() << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void AstPrinter::printNode(AssignNode *node, int level) {
        std::cout << "assign " << node->getName() << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void AstPrinter::printNode(VarNode *node, int level) {
        std::cout << "var " << node->getName() << std::endl;
    }

    void AstPrinter::printNode(IfNode *node, int level) {
        std::cout << "if" << std::endl;

        node->getCond()->print(*this, level + 1);
        node->getThenNode()->print(*this, level + 1);
        if (node->getElseNode()) {
            node->getElseNode()->print(*this, level + 1);
        }
    }

    void AstPrinter::printNode(WhileNode *node, int level) {
        std::cout << "while" << std::endl;

        node->getCond()->print(*this, level + 1);
        node->getBody()->print(*this, level + 1);
    }

    void AstPrinter::printNode(ArgNode *node, int level) {
        std::cout << "arg " << node->getType() << " " << node->getName() << std::endl;
    }

    void AstPrinter::printNode(FnNode *node, int level) {
        std::cout << "fn " << node->getName() << " -> " << node->getReturnType() << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
        node->getBody()->print(*this, level + 1);
    }

    void AstPrinter::printNode(FnCallNode *node, int level) {
        std::cout << "fn call " << node->getName() << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void AstPrinter::printNode(ReturnNode *node, int level) {
        std::cout << "return" << std::endl;

        node->getVal()->print(*this, level + 1);
    }

    void AstPrinter::printNode(BreakNode *node, int level) {
        std::cout << "break" << std::endl;
    }

    void AstPrinter::printNode(ContinueNode *node, int level) {
        std::cout << "continue" << std::endl;
    }

    void AstPrinter::printNode(CommentNode *node, int level) {
        std::cout << "// " << node->getComment() << std::endl;
    }
}
