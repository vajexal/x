#ifndef X_AST_PRINTER_H
#define X_AST_PRINTER_H

#include "ast.h"

namespace X {
    class AstPrinter {
    public:
        template<class T>
        void print(T *node, int level = 0) {
            std::cout << std::string(level * 2, ' ');

            printNode(node, level);
        }

    private:
        void printNode(Node *node, int level);
        void printNode(StatementListNode *node, int level);
        void printNode(ScalarNode *node, int level);
        void printNode(UnaryNode *node, int level);
        void printNode(BinaryNode *node, int level);
        void printNode(DeclareNode *node, int level);
        void printNode(AssignNode *node, int level);
        void printNode(VarNode *node, int level);
        void printNode(IfNode *node, int level);
        void printNode(WhileNode *node, int level);
        void printNode(ArgNode *node, int level);
        void printNode(FnNode *node, int level);
        void printNode(FnCallNode *node, int level);
        void printNode(ReturnNode *node, int level);
        void printNode(BreakNode *node, int level);
        void printNode(ContinueNode *node, int level);
        void printNode(CommentNode *node, int level);
    };

    class AstPrinterException : public std::exception {
        const char *message;

    public:
        AstPrinterException(const char *m) : message(m) {}

        const char *what() const noexcept override {
            return message;
        }
    };
};

#endif //X_AST_PRINTER_H
