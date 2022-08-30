#ifndef X_AST_H
#define X_AST_H

#include <iostream>
#include <vector>
#include <variant>
#include <optional>

#include "llvm/IR/Value.h"

namespace X {
    using ScalarValue = std::variant<int, float, bool>;

    enum class OpType {
        PRE_INC,
        PRE_DEC,
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

    class Type {
    public:
        enum class TypeID {
            INT,
            FLOAT,
            BOOL,
            VOID,
            CLASS
        };
    private:
        TypeID id;
        std::optional<std::string> className;

    public:
        Type() : id(TypeID::VOID) {} // need empty constructor for bison variant
        explicit Type(TypeID typeID) : id(typeID) {
            if (typeID == TypeID::CLASS) {
                throw std::invalid_argument("invalid type for scalar");
            }
        }
        explicit Type(std::string className) : id(TypeID::CLASS), className(std::move(className)) {}

        TypeID getTypeID() const { return id; }
        const std::string &getClassName() const { return className.value(); }
    };

    std::ostream &operator<<(std::ostream &out, const Type &type);

    enum class AccessModifier {
        PUBLIC,
        PRIVATE
    };

    std::ostream &operator<<(std::ostream &out, AccessModifier accessModifier);

    class AstPrinter;

    class Codegen;

    class Node {
    public:
        virtual ~Node() = default;

        virtual void print(AstPrinter &astPrinter, int level = 0) = 0;
        virtual llvm::Value *gen(Codegen &codegen) = 0;

        virtual bool isTerminate() { return false; }
    };

    class ExprNode : public Node {
    };

    class ScalarNode : public ExprNode {
        Type type;
        ScalarValue value;

    public:
        ScalarNode(Type type, const ScalarValue &value) : type(std::move(type)), value(value) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const Type &getType() const { return type; }
        const ScalarValue &getValue() const { return value; }
    };

    class StatementListNode : public Node {
        std::vector<Node *> children;

    public:
        ~StatementListNode() {
            for (auto child: children) {
                delete child;
            }
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        void add(Node *node) { children.push_back(node); }
        bool isLastNodeTerminate() const { return children.back()->isTerminate(); }
        const std::vector<Node *> &getChildren() const { return children; }
    };

    class UnaryNode : public ExprNode {
        OpType type;
        ExprNode *expr;

    public:
        UnaryNode(OpType type, ExprNode *expr) : type(type), expr(expr) {}
        ~UnaryNode() {
            delete expr;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        OpType getType() const { return type; }
        ExprNode *getExpr() const { return expr; }
    };

    class BinaryNode : public ExprNode {
        OpType type;
        ExprNode *lhs;
        ExprNode *rhs;

    public:
        BinaryNode(OpType type, ExprNode *lhs, ExprNode *rhs) : type(type), lhs(lhs), rhs(rhs) {}
        ~BinaryNode() {
            delete lhs;
            delete rhs;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        OpType getType() const { return type; }
        ExprNode *getLhs() const { return lhs; }
        ExprNode *getRhs() const { return rhs; }
    };

    class DeclareNode : public Node {
        Type type;
        std::string name;
        ExprNode *expr;

    public:
        DeclareNode(Type type, std::string name, ExprNode *expr) : type(std::move(type)), name(std::move(name)), expr(expr) {}
        ~DeclareNode() {
            delete expr;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const Type &getType() const { return type; }
        const std::string &getName() const { return name; }
        ExprNode *getExpr() const { return expr; }
    };

    class AssignNode : public Node {
        std::string name;
        ExprNode *expr;

    public:
        AssignNode(std::string name, ExprNode *expr) : name(std::move(name)), expr(expr) {}
        ~AssignNode() {
            delete expr;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getName() const { return name; }
        ExprNode *getExpr() const { return expr; }
    };

    class VarNode : public ExprNode {
        std::string name;

    public:
        VarNode(std::string name) : name(std::move(name)) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getName() const { return name; }
    };

    class IfNode : public Node {
        ExprNode *cond;
        StatementListNode *thenNode;
        StatementListNode *elseNode;

    public:
        IfNode(ExprNode *cond, StatementListNode *thenNode, StatementListNode *elseNode = nullptr) : cond(cond), thenNode(thenNode), elseNode(elseNode) {}
        ~IfNode() {
            delete cond;
            delete thenNode;
            delete elseNode;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        ExprNode *getCond() const { return cond; }
        StatementListNode *getThenNode() const { return thenNode; }
        StatementListNode *getElseNode() const { return elseNode; }
    };

    class WhileNode : public Node {
        ExprNode *cond;
        StatementListNode *body;

    public:
        WhileNode(ExprNode *cond, StatementListNode *body) : cond(cond), body(body) {}
        ~WhileNode() {
            delete cond;
            delete body;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        ExprNode *getCond() const { return cond; }
        StatementListNode *getBody() const { return body; }
    };

    class ArgNode : public Node {
        Type type;
        std::string name;

    public:
        ArgNode(Type type, std::string name) : type(std::move(type)), name(std::move(name)) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const Type &getType() const { return type; }
        const std::string &getName() const { return name; }
    };

    class FnNode : public Node {
        std::string name;
        std::vector<ArgNode *> args;
        Type returnType;
        StatementListNode *body;

    public:
        FnNode(std::string name, std::vector<ArgNode *> args, Type returnType, StatementListNode *body) :
                name(std::move(name)), args(std::move(args)), returnType(std::move(returnType)), body(body) {}

        ~FnNode() {
            for (auto arg: args) {
                delete arg;
            }

            delete body;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const std::vector<ArgNode *> &getArgs() const { return args; }
        const Type &getReturnType() const { return returnType; }
        StatementListNode *getBody() const { return body; }
    };

    class FnCallNode : public ExprNode {
        std::string name;
        std::vector<ExprNode *> args;

    public:
        FnCallNode(std::string name, std::vector<ExprNode *> args) : name(std::move(name)), args(std::move(args)) {}
        ~FnCallNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const std::vector<ExprNode *> &getArgs() const { return args; }
    };

    class ReturnNode : public Node {
        ExprNode *val;

    public:
        explicit ReturnNode(ExprNode *val = nullptr) : val(val) {}
        ~ReturnNode() {
            delete val;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        bool isTerminate() override { return true; };
        ExprNode *getVal() const { return val; }
    };

    class BreakNode : public Node {
    public:
        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        bool isTerminate() override { return true; };
    };

    class ContinueNode : public Node {
    public:
        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        bool isTerminate() override { return true; };
    };

    class CommentNode : public Node {
        std::string comment;

    public:
        CommentNode(std::string comment) : comment(std::move(comment)) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getComment() const { return comment; }
    };

    class PropDeclNode : public Node {
        Type type;
        std::string name;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        PropDeclNode(Type type, std::string name, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) :
                type(std::move(type)), name(std::move(name)), accessModifier(accessModifier), isStatic(isStatic) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const Type &getType() const { return type; }
        const std::string &getName() const { return name; }
        AccessModifier getAccessModifier() const { return accessModifier; }
        bool getIsStatic() const { return isStatic; }
    };

    class MethodDeclNode : public Node {
        FnNode *fn;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        MethodDeclNode(FnNode *fn, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) : fn(fn), accessModifier(accessModifier), isStatic(isStatic) {}
        ~MethodDeclNode() {
            delete fn;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        FnNode *getFn() const { return fn; }
        bool getIsStatic() const { return isStatic; }
        AccessModifier getAccessModifier() const { return accessModifier; }
    };

    class ClassMembersNode : public Node {
        std::vector<PropDeclNode *> props;
        std::vector<MethodDeclNode *> methods;

    public:
        ~ClassMembersNode() {
            for (auto prop: props) {
                delete prop;
            }
            for (auto method: methods) {
                delete method;
            }
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        void addProp(PropDeclNode *prop) { props.push_back(prop); }
        void addMethod(MethodDeclNode *fn) { methods.push_back(fn); }
        const std::vector<PropDeclNode *> &getProps() const { return props; }
        const std::vector<MethodDeclNode *> &getMethods() const { return methods; }
    };

    class ClassNode : public Node {
        std::string name;
        ClassMembersNode *members;

    public:
        ClassNode(std::string name, ClassMembersNode *members) : name(std::move(name)), members(members) {}
        ~ClassNode() {
            delete members;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getName() const { return name; }
        ClassMembersNode *getMembers() { return members; }
    };

    class FetchPropNode : public ExprNode {
        VarNode *obj;
        std::string name;

    public:
        FetchPropNode(VarNode *obj, std::string name) : obj(obj), name(std::move(name)) {}
        ~FetchPropNode() {
            delete obj;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        VarNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
    };

    class FetchStaticPropNode : public ExprNode {
        std::string className;
        std::string propName;

    public:
        FetchStaticPropNode(std::string className, std::string propName) : className(std::move(className)), propName(std::move(propName)) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getClassName() const { return className; }
        const std::string &getPropName() const { return propName; }
    };

    class MethodCallNode : public ExprNode {
        VarNode *obj;
        std::string name;
        std::vector<ExprNode *> args;

    public:
        MethodCallNode(VarNode *obj, std::string name, std::vector<ExprNode *> args) : obj(obj), name(std::move(name)), args(std::move(args)) {}
        ~MethodCallNode() {
            delete obj;

            for (auto expr: args) {
                delete expr;
            }
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        VarNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
        const std::vector<ExprNode *> &getArgs() const { return args; }
    };

    class StaticMethodCallNode : public ExprNode {
        std::string className;
        std::string methodName;
        std::vector<ExprNode *> args;

    public:
        StaticMethodCallNode(std::string className, std::string methodName, std::vector<ExprNode *> args) :
                className(std::move(className)), methodName(std::move(methodName)), args(std::move(args)) {}

        ~StaticMethodCallNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getClassName() const { return className; }
        const std::string &getMethodName() const { return methodName; }
        const std::vector<ExprNode *> &getArgs() const { return args; }
    };

    class AssignPropNode : public Node {
        VarNode *obj;
        std::string name;
        ExprNode *expr;

    public:
        AssignPropNode(VarNode *obj, std::string name, ExprNode *expr) : obj(obj), name(std::move(name)), expr(expr) {}
        ~AssignPropNode() {
            delete obj;
            delete expr;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        VarNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
        ExprNode *getExpr() const { return expr; }
    };

    class AssignStaticPropNode : public Node {
        std::string className;
        std::string propName;
        ExprNode *expr;

    public:
        AssignStaticPropNode(std::string className, std::string propName, ExprNode *expr) :
                className(std::move(className)), propName(std::move(propName)), expr(expr) {}

        ~AssignStaticPropNode() {
            delete expr;
        }

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        const std::string &getClassName() const { return className; }
        const std::string &getPropName() const { return propName; }
        ExprNode *getExpr() const { return expr; }
    };

    class NewNode : public ExprNode {
        std::string name;

    public:
        NewNode(std::string name) : name(std::move(name)) {}

        void print(AstPrinter &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen &codegen) override;

        std::string getName() const { return name; }
    };
}

#endif //X_AST_H
