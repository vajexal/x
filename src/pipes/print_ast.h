#pragma once

#include "pipeline.h"
#include "ast.h"

namespace X::Pipes {
    class PrintAst : public Pipe {
    public:
        TopStatementListNode *handle(X::TopStatementListNode *node) override {
            print<StatementListNode>(node);

            return node;
        }

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
        void printNode(DeclNode *node, int level);
        void printNode(AssignNode *node, int level);
        void printNode(VarNode *node, int level);
        void printNode(IfNode *node, int level);
        void printNode(WhileNode *node, int level);
        void printNode(ForNode *node, int level);
        void printNode(RangeNode *node, int level);
        void printNode(BreakNode *node, int level);
        void printNode(ContinueNode *node, int level);
        void printNode(ArgNode *node, int level);
        void printNode(FnDeclNode *node, int level);
        void printNode(FnDefNode *node, int level);
        void printNode(FnCallNode *node, int level);
        void printNode(ReturnNode *node, int level);
        void printNode(PrintlnNode *node, int level);
        void printNode(CommentNode *node, int level);
        void printNode(ClassNode *node, int level);
        void printNode(PropDeclNode *node, int level);
        void printNode(MethodDefNode *node, int level);
        void printNode(FetchPropNode *node, int level);
        void printNode(FetchStaticPropNode *node, int level);
        void printNode(MethodCallNode *node, int level);
        void printNode(StaticMethodCallNode *node, int level);
        void printNode(AssignPropNode *node, int level);
        void printNode(AssignStaticPropNode *node, int level);
        void printNode(NewNode *node, int level);
        void printNode(MethodDeclNode *node, int level);
        void printNode(InterfaceNode *node, int level);
        void printNode(FetchArrNode *node, int level);
        void printNode(AssignArrNode *node, int level);
        void printNode(AppendArrNode *node, int level);
    };

    class PrintAstException : public std::exception {
        const char *message;

    public:
        PrintAstException(const char *m) : message(m) {}

        const char *what() const noexcept override {
            return message;
        }
    };
}
