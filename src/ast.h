#ifndef X_AST_H
#define X_AST_H

#include <iostream>
#include <vector>
#include <variant>

#include "llvm/IR/Value.h"

namespace X {
    using ScalarValue = std::variant<int, float, bool>;

    enum class OpType {
        POST_INC,
        POST_DEC,
        OR,
        AND,
        PLUS,
        MINUS,
        MUL,
        DIV,
        NOT,
        POW,
        EQUAL,
        NOT_EQUAL,
        SMALLER,
        SMALLER_OR_EQUAL,
        GREATER,
        GREATER_OR_EQUAL
    };

    std::ostream &operator<<(std::ostream &out, OpType type);

    enum class Type {
        INT,
        FLOAT,
        BOOL,
        VOID
    };

    std::ostream &operator<<(std::ostream &out, Type type);

    class AstPrinter;
    class Codegen;

    class Node {
    public:
        virtual void print(AstPrinter &astPrinter, int level = 0) = 0;
        virtual llvm::Value *gen(Codegen &codegen) = 0;
        virtual bool isTerminate() {
            return false;
        }
    };

    class ExprNode : public Node {
    public:
        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);
    };

    class ScalarNode : public ExprNode {
        Type type;
        ScalarValue value;

    public:
        ScalarNode(Type type, const ScalarValue &value) : type(type), value(value) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        Type getType() const {
            return type;
        }

        const ScalarValue &getValue() const {
            return value;
        }
    };

    class StatementListNode : public Node {
        std::vector<Node *> children;

    public:
        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        void add(Node *node) {
            children.push_back(node);
        }

        bool isLastNodeTerminate() {
            return children.back()->isTerminate();
        }

        auto getChildren() {
            return children;
        }
    };

    class UnaryNode : public ExprNode {
        OpType type;
        ExprNode *expr;

    public:
        UnaryNode(OpType type, ExprNode *expr) : type(type), expr(expr) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        OpType getType() const {
            return type;
        }

        ExprNode *getExpr() const {
            return expr;
        }
    };

    class BinaryNode : public ExprNode {
        OpType type;
        ExprNode *lhs;
        ExprNode *rhs;

    public:
        BinaryNode(OpType type, ExprNode *lhs, ExprNode *rhs) : type(type), lhs(lhs), rhs(rhs) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        OpType getType() const {
            return type;
        }

        ExprNode *getLhs() const {
            return lhs;
        }

        ExprNode *getRhs() const {
            return rhs;
        }
    };

    class DeclareNode : public Node {
        Type type;
        std::string name;
        ExprNode *expr;

    public:
        DeclareNode(Type type, const std::string &name, ExprNode *expr) : type(type), name(name), expr(expr) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        Type getType() const {
            return type;
        }

        const std::string &getName() const {
            return name;
        }

        ExprNode *getExpr() const {
            return expr;
        }
    };

    class AssignNode : public Node {
        std::string name;
        ExprNode *expr;

    public:
        AssignNode(const std::string &name, ExprNode *expr) : name(name), expr(expr) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getName() const {
            return name;
        }

        ExprNode *getExpr() const {
            return expr;
        }
    };

    class VarNode : public ExprNode {
        std::string name;

    public:
        VarNode(const std::string &name) : name(name) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getName() const {
            return name;
        }
    };

    class IfNode : public Node {
        ExprNode *cond;
        StatementListNode *thenNode;
        StatementListNode *elseNode;

    public:
        IfNode(ExprNode *cond, StatementListNode *thenNode) : cond(cond), thenNode(thenNode) {}
        IfNode(ExprNode *cond, StatementListNode *thenNode, StatementListNode *elseNode) : cond(cond), thenNode(thenNode), elseNode(elseNode) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        ExprNode *getCond() const {
            return cond;
        }

        StatementListNode *getThenNode() const {
            return thenNode;
        }

        StatementListNode *getElseNode() const {
            return elseNode;
        }
    };

    class WhileNode : public Node {
        ExprNode *cond;
        StatementListNode *body;

    public:
        WhileNode(ExprNode *cond, StatementListNode *body) : cond(cond), body(body) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        ExprNode *getCond() const {
            return cond;
        }

        StatementListNode *getBody() const {
            return body;
        }
    };

    class ArgNode : public Node {
        Type type;
        std::string name;

    public:
        ArgNode(Type type, const std::string &name) : type(type), name(name) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        Type getType() const {
            return type;
        }

        const std::string &getName() const {
            return name;
        }
    };

    class FnNode : public Node {
        std::string name;
        std::vector<ArgNode *> args;
        Type returnType;
        StatementListNode *body;

    public:
        FnNode(const std::string &name, const std::vector<ArgNode *> &args, Type returnType, StatementListNode *body) : name(name), args(args),
                                                                                                                        returnType(returnType),
                                                                                                                        body(body) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getName() const {
            return name;
        }

        const std::vector<ArgNode *> &getArgs() const {
            return args;
        }

        Type getReturnType() const {
            return returnType;
        }

        StatementListNode *getBody() const {
            return body;
        }
    };

    class FnCallNode : public ExprNode {
        std::string name;
        std::vector<ExprNode *> args;

    public:
        FnCallNode(const std::string &name, const std::vector<ExprNode *> &args) : name(name), args(args) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getName() const {
            return name;
        }

        const std::vector<ExprNode *> &getArgs() const {
            return args;
        }
    };

    class ReturnNode : public Node {
        ExprNode *val;

    public:
        explicit ReturnNode(ExprNode *val) : val(val) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        bool isTerminate() override {
            return true;
        };

        ExprNode *getVal() const {
            return val;
        }
    };

    class BreakNode : public Node {
    public:
        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);
    };

    class ContinueNode : public Node {
    public:
        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);
    };

    class CommentNode: public Node {
        std::string comment;
    public:
        CommentNode(const std::string &comment): comment(comment) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getComment() const {
            return comment;
        }
    };
}

#endif //X_AST_H
