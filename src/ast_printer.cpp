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
        std::cout << "var " << node->getType() << ' ' << node->getName() << " = " << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void AstPrinter::printNode(AssignNode *node, int level) {
        std::cout << node->getName() << " =" << std::endl;

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
        std::cout << "arg " << node->getType() << ' ' << node->getName() << std::endl;
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

        if (node->getVal()) {
            node->getVal()->print(*this, level + 1);
        }
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

    void AstPrinter::printNode(ClassNode *node, int level) {
        std::cout << "class " << node->getName() << std::endl;

        node->getMembers().print(*this, level + 1);
    }

    void AstPrinter::printNode(ClassMembersNode *node, int level) {
        for (auto &prop: node->getProps()) {
            prop->print(*this, level);
        }

        for (auto &method: node->getMethods()) {
            method->print(*this, level);
        }
    }

    void AstPrinter::printNode(PropDeclNode *node, int level) {
        std::cout << node->getAccessModifier() << ' ';

        if (node->getIsStatic()) {
            std::cout << "static ";
        }

        std::cout << node->getType() << ' ' << node->getName() << std::endl;
    }

    void AstPrinter::printNode(MethodDeclNode *node, int level) {
        std::cout << node->getAccessModifier() << ' ';

        if (node->getIsStatic()) {
            std::cout << "static ";
        }

        node->getFn()->print(*this, level);
    }

    void AstPrinter::printNode(FetchPropNode *node, int level) {
        std::cout << '.' << node->getName() << std::endl;

        node->getObj()->print(*this, level + 1);
    }

    void AstPrinter::printNode(FetchStaticPropNode *node, int level) {
        std::cout << node->getClassName() << "::" << node->getPropName() << std::endl;
    }

    void AstPrinter::printNode(MethodCallNode *node, int level) {
        std::cout << '.' << node->getName() << "()" << std::endl;

        node->getObj()->print(*this, level + 1);

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void AstPrinter::printNode(StaticMethodCallNode *node, int level) {
        std::cout << node->getClassName() << "::" << node->getMethodName() << "()" << std::endl;

        for (auto &arg: node->getArgs()) {
            arg->print(*this, level + 1);
        }
    }

    void AstPrinter::printNode(AssignPropNode *node, int level) {
        std::cout << '.' << node->getName() << " =" << std::endl;

        node->getObj()->print(*this, level + 1);
        node->getExpr()->print(*this, level + 1);
    }

    void AstPrinter::printNode(AssignStaticPropNode *node, int level) {
        std::cout << node->getClassName() << "::" << node->getPropName() << " =" << std::endl;

        node->getExpr()->print(*this, level + 1);
    }

    void AstPrinter::printNode(NewNode *node, int level) {
        std::cout << "new " << node->getName() << std::endl;
    }
}
