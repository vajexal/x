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
        explicit Type(TypeID typeID) : id(typeID) {
            if (typeID == TypeID::CLASS) {
                throw std::invalid_argument("invalid type for scalar");
            }
        }
        explicit Type(const std::string &className) : id(TypeID::CLASS), className(className) {}

        const TypeID &getTypeID() const { return id; }
        const std::optional<std::string> &getClassName() const { return className; }
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

        const Type &getType() const {
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

        const Type &getType() const {
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

        bool isTerminate() override {
            return true;
        };
    };

    class ContinueNode : public Node {
    public:
        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        bool isTerminate() override {
            return true;
        };
    };

    class CommentNode : public Node {
        std::string comment;

    public:
        CommentNode(const std::string &comment) : comment(comment) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getComment() const {
            return comment;
        }
    };

    class PropDeclNode : public Node {
        Type type;
        std::string name;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        PropDeclNode(const Type &type, const std::string &name, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) : type(type), name(name),
                                                                                                                                                 accessModifier(accessModifier),
                                                                                                                                                 isStatic(isStatic) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

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

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        FnNode *getFn() const { return fn; }
        bool getIsStatic() const { return isStatic; }
        AccessModifier getAccessModifier() const { return accessModifier; }
    };

    class ClassMembersNode : public Node {
        std::vector<PropDeclNode *> props;
        std::vector<MethodDeclNode *> methods;

    public:
        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        void addProp(PropDeclNode *prop) { props.push_back(prop); }
        void addMethod(MethodDeclNode *fn) { methods.push_back(fn); }
        const std::vector<PropDeclNode *> &getProps() const { return props; }
        const std::vector<MethodDeclNode *> &getMethods() const { return methods; }
    };

    class ClassNode : public Node {
        std::string name;
        ClassMembersNode members;

    public:
        ClassNode(const std::string &name, const ClassMembersNode &members) : name(name), members(members) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getName() const { return name; }
        ClassMembersNode &getMembers() { return members; }
    };

    class FetchPropNode : public ExprNode {
        VarNode *obj;
        std::string name;

    public:
        FetchPropNode(VarNode *obj, const std::string &name) : obj(obj), name(name) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        VarNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
    };

    class FetchStaticPropNode : public ExprNode {
        std::string className;
        std::string propName;

    public:
        FetchStaticPropNode(const std::string &className, const std::string &propName) : className(className), propName(propName) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getClassName() const { return className; }
        const std::string &getPropName() const { return propName; }
    };

    class MethodCallNode : public ExprNode {
        VarNode *obj;
        std::string name;
        std::vector<ExprNode *> &args;

    public:
        MethodCallNode(VarNode *obj, std::string &name, std::vector<ExprNode *> &args) : obj(obj), name(name), args(args) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        VarNode *getObj() const { return obj; }
        std::string getName() const { return name; }
        const std::vector<ExprNode *> &getArgs() const { return args; }
    };

    class StaticMethodCallNode : public ExprNode {
        std::string className;
        std::string methodName;
        std::vector<ExprNode *> &args;

    public:
        StaticMethodCallNode(const std::string &className, const std::string &methodName, std::vector<ExprNode *> &args) : className(className), methodName(methodName),
                                                                                                                           args(args) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getClassName() const { return className; }
        const std::string &getMethodName() const { return methodName; }
        const std::vector<ExprNode *> &getArgs() const { return args; }
    };

    class AssignPropNode : public Node {
        VarNode *obj;
        std::string name;
        ExprNode *expr;

    public:
        AssignPropNode(VarNode *obj, const std::string &name, ExprNode *expr) : obj(obj), name(name), expr(expr) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        VarNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
        ExprNode *getExpr() const { return expr; }
    };

    class AssignStaticPropNode : public Node {
        std::string className;
        std::string propName;
        ExprNode *expr;

    public:
        AssignStaticPropNode(const std::string &className, const std::string &propName, ExprNode *expr) : className(className), propName(propName), expr(expr) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        const std::string &getClassName() const { return className; }
        const std::string &getPropName() const { return propName; }
        ExprNode *getExpr() const { return expr; }
    };

    class NewNode : public ExprNode {
        std::string name;

    public:
        NewNode(std::string name) : name(name) {}

        void print(AstPrinter &astPrinter, int level = 0);
        llvm::Value *gen(Codegen &codegen);

        std::string getName() const { return name; }
    };
}

#endif //X_AST_H
