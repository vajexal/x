#ifndef X_AST_H
#define X_AST_H

#include <iostream>
#include <utility>
#include <vector>
#include <deque>
#include <map>
#include <variant>
#include <optional>
#include <string>
#include <fmt/core.h>

#include "llvm/IR/Value.h"

namespace X {
    class ExprNode;

    using ExprList = std::vector<ExprNode *>;
    using ScalarValue = std::variant<int64_t, double, bool, std::string, ExprList>;

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
            STRING,
            ARRAY,
            VOID,
            CLASS
        };
    private:
        TypeID id;
        std::optional<std::string> className;
        Type *subtype = nullptr;

    public:
        Type() : id(TypeID::VOID) {} // need empty constructor for bison variant

        static Type scalar(TypeID typeID);
        static Type klass(std::string className);
        static Type array(Type *subtype);

        TypeID getTypeID() const { return id; }
        const std::string &getClassName() const { return className.value(); }
        Type *getSubtype() const { return subtype; }

        bool operator==(const Type &other) const;
        bool operator!=(const Type &other) const;
    };

    std::ostream &operator<<(std::ostream &out, const Type &type);

    enum class AccessModifier {
        PUBLIC,
        PROTECTED,
        PRIVATE
    };

    std::ostream &operator<<(std::ostream &out, AccessModifier accessModifier);

    class AstException : public std::exception {
        std::string message;

    public:
        AstException(const char *m) : message(m) {}
        AstException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };

    class MethodAlreadyDeclaredException : public AstException {
    public:
        MethodAlreadyDeclaredException(const std::string &className, const std::string &methodName) :
                AstException(fmt::format("method {}::{} already declared", className, methodName)) {}
    };

    namespace Pipes {
        class PrintAst;
    }
    namespace Codegen {
        class Codegen;
    }

    class Node {
    public:
        virtual ~Node() = default;

        virtual void print(Pipes::PrintAst &astPrinter, int level = 0) = 0;
        virtual llvm::Value *gen(Codegen::Codegen &codegen) = 0;

        virtual bool isTerminate() { return false; }
    };

    class ExprNode : public Node {
    };

    class ScalarNode : public ExprNode {
        Type type;
        ScalarValue value;

    public:
        ScalarNode(Type type, ScalarValue value) : type(std::move(type)), value(std::move(value)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const Type &getType() const { return type; }
        const ScalarValue &getValue() const { return value; }
    };

    class StatementListNode : public Node {
        std::deque<Node *> children;

    public:
        ~StatementListNode() {
            for (auto child: children) {
                delete child;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        void add(Node *node) { children.push_back(node); }
        void prepend(Node *node) { children.push_front(node); }
        bool isLastNodeTerminate() const;
        const std::deque<Node *> &getChildren() const { return children; }
    };

    class UnaryNode : public ExprNode {
        OpType type;
        ExprNode *expr;

    public:
        UnaryNode(OpType type, ExprNode *expr) : type(type), expr(expr) {}
        ~UnaryNode() {
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

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

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        OpType getType() const { return type; }
        ExprNode *getLhs() const { return lhs; }
        ExprNode *getRhs() const { return rhs; }
    };

    class DeclareNode : public Node {
        Type type;
        std::string name;
        ExprNode *expr;

    public:
        DeclareNode(Type type, std::string name, ExprNode *expr = nullptr) : type(std::move(type)), name(std::move(name)), expr(expr) {}
        ~DeclareNode() {
            if (expr) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

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

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        ExprNode *getExpr() const { return expr; }
    };

    class VarNode : public ExprNode {
        std::string name;

    public:
        VarNode(std::string name) : name(std::move(name)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

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

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

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

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getCond() const { return cond; }
        StatementListNode *getBody() const { return body; }
    };

    class ForNode : public Node {
        std::optional<std::string> idx;
        std::string val;
        ExprNode *expr;
        StatementListNode *body;

    public:
        ForNode(std::string val, ExprNode *expr, StatementListNode *body) : val(std::move(val)), expr(expr), body(body) {}
        ForNode(std::string idx, std::string val, ExprNode *expr, StatementListNode *body) :
                idx(std::move(idx)), val(std::move(val)), expr(expr), body(body) {}
        ~ForNode() {
            delete expr;
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getIdx() const { return idx.value(); }
        const std::string &getVal() const { return val; }
        ExprNode *getExpr() const { return expr; }
        StatementListNode *getBody() const { return body; }

        bool hasIdx() const { return idx.has_value(); }
    };

    class RangeNode : public ExprNode {
        ExprNode *start = nullptr;
        ExprNode *stop;
        ExprNode *step = nullptr;

    public:
        RangeNode(ExprNode *stop) : stop(stop) {}
        RangeNode(ExprNode *start, ExprNode *stop, ExprNode *step = nullptr) : start(start), stop(stop), step(step) {}
        ~RangeNode() {
            delete start;
            delete stop;
            delete step;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getStart() const { return start; }
        ExprNode *getStop() const { return stop; }
        ExprNode *getStep() const { return step; }
    };

    class ArgNode : public Node {
        Type type;
        std::string name;

    public:
        ArgNode(Type type, std::string name) : type(std::move(type)), name(std::move(name)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const Type &getType() const { return type; }
        const std::string &getName() const { return name; }

        bool operator==(const ArgNode &other) const;
        bool operator!=(const ArgNode &other) const;
    };

    class FnDeclNode : public Node {
        std::string name;
        std::vector<ArgNode *> args;
        Type returnType;

    public:
        FnDeclNode(std::string name, std::vector<ArgNode *> args, Type returnType) : name(std::move(name)), args(std::move(args)),
                                                                                     returnType(std::move(returnType)) {}
        ~FnDeclNode() {
            for (auto arg: args) {
                delete arg;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const std::vector<ArgNode *> &getArgs() const { return args; }
        const Type &getReturnType() const { return returnType; }

        bool operator==(const FnDeclNode &other) const;
        bool operator!=(const FnDeclNode &other) const;
    };

    class FnDefNode : public Node {
        std::string name;
        std::vector<ArgNode *> args;
        Type returnType; // todo FnDeclNode
        StatementListNode *body;

    public:
        FnDefNode(std::string name, std::vector<ArgNode *> args, Type returnType, StatementListNode *body) :
                name(std::move(name)), args(std::move(args)), returnType(std::move(returnType)), body(body) {}

        ~FnDefNode() {
            for (auto arg: args) {
                delete arg;
            }

            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const std::vector<ArgNode *> &getArgs() const { return args; }
        const Type &getReturnType() const { return returnType; }
        StatementListNode *getBody() const { return body; }

        bool operator==(const FnDefNode &other) const;
        bool operator!=(const FnDefNode &other) const;
    };

    class FnCallNode : public ExprNode {
        std::string name;
        ExprList args;

    public:
        FnCallNode(std::string name, ExprList args) : name(std::move(name)), args(std::move(args)) {}
        ~FnCallNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const ExprList &getArgs() const { return args; }
    };

    class ReturnNode : public Node {
        ExprNode *val;

    public:
        explicit ReturnNode(ExprNode *val = nullptr) : val(val) {}
        ~ReturnNode() {
            delete val;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        bool isTerminate() override { return true; };
        ExprNode *getVal() const { return val; }
    };

    class PrintlnNode : public Node {
        ExprNode *val;

    public:
        explicit PrintlnNode(ExprNode *val) : val(val) {}
        ~PrintlnNode() {
            delete val;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getVal() const { return val; }
    };

    class BreakNode : public Node {
    public:
        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        bool isTerminate() override { return true; };
    };

    class ContinueNode : public Node {
    public:
        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        bool isTerminate() override { return true; };
    };

    class CommentNode : public Node {
        std::string comment;

    public:
        CommentNode(std::string comment) : comment(std::move(comment)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

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

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const Type &getType() const { return type; }
        const std::string &getName() const { return name; }
        AccessModifier getAccessModifier() const { return accessModifier; }
        bool getIsStatic() const { return isStatic; }
    };

    class MethodDeclNode : public Node {
        FnDeclNode *fnDecl;
        bool isAbstract;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        MethodDeclNode(FnDeclNode *fnDecl, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) :
                fnDecl(fnDecl), accessModifier(accessModifier), isStatic(isStatic) {}
        ~MethodDeclNode() {
            delete fnDecl;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        FnDeclNode *getFnDecl() const { return fnDecl; }
        bool getIsAbstract() const { return isAbstract; }
        void setAbstract() { isAbstract = true; } // todo remove this method
        bool getIsStatic() const { return isStatic; }
        AccessModifier getAccessModifier() const { return accessModifier; }

        bool operator==(const MethodDeclNode &other) const;
        bool operator!=(const MethodDeclNode &other) const;
    };

    class MethodDefNode : public Node {
        FnDefNode *fnDef;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        MethodDefNode(FnDefNode *fnDef, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) :
                fnDef(fnDef), accessModifier(accessModifier), isStatic(isStatic) {}
        ~MethodDefNode() {
            delete fnDef;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        FnDefNode *getFnDef() const { return fnDef; }
        bool getIsStatic() const { return isStatic; }
        AccessModifier getAccessModifier() const { return accessModifier; }

        bool operator==(const MethodDefNode &other) const;
        bool operator!=(const MethodDefNode &other) const;
    };

    class ClassMembersNode : public Node {
        std::deque<Node *> children;
        std::vector<PropDeclNode *> props;
        std::vector<MethodDefNode *> methods;
        std::vector<MethodDeclNode *> abstractMethods;

    public:
        ~ClassMembersNode() {
            for (auto child: children) {
                delete child;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        void addComment(CommentNode *node) {
            children.push_back(node);
        }
        void addProp(PropDeclNode *prop) {
            children.push_back(prop);
            props.push_back(prop);
        }
        void addMethod(MethodDefNode *fnDef) {
            children.push_back(fnDef);
            methods.push_back(fnDef);
        }
        void addAbstractMethod(MethodDeclNode *fnDecl) {
            children.push_back(fnDecl);
            abstractMethods.push_back(fnDecl);
        }
        const std::deque<Node *> &getChildren() const { return children; }
        const std::vector<PropDeclNode *> &getProps() const { return props; }
        const std::vector<MethodDefNode *> &getMethods() const { return methods; }
        const std::vector<MethodDeclNode *> &getAbstractMethods() const { return abstractMethods; }
    };

    class ClassNode : public Node {
        std::string name;
        StatementListNode *body;
        std::vector<PropDeclNode *> props;
        std::map<std::string, MethodDefNode *> methods;
        std::map<std::string, MethodDeclNode *> abstractMethods;
        std::string parent;
        std::vector<std::string> interfaces;
        bool abstract;

    public:
        ClassNode(std::string name, StatementListNode *body, std::string parent, std::vector<std::string> interfaces, bool abstract);
        ~ClassNode() {
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        StatementListNode *getBody() { return body; }
        const std::vector<PropDeclNode *> &getProps() const { return props; }
        const std::map<std::string, MethodDefNode *> &getMethods() const { return methods; }
        const std::map<std::string, MethodDeclNode *> &getAbstractMethods() const { return abstractMethods; }
        const std::string &getParent() const { return parent; }
        const std::vector<std::string> &getInterfaces() const { return interfaces; }
        bool isAbstract() const { return abstract; }
        bool hasParent() const { return !parent.empty(); }
    };

    class FetchPropNode : public ExprNode {
        ExprNode *obj;
        std::string name;

    public:
        FetchPropNode(ExprNode *obj, std::string name) : obj(obj), name(std::move(name)) {}
        ~FetchPropNode() {
            delete obj;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
    };

    class FetchStaticPropNode : public ExprNode {
        std::string className;
        std::string propName;

    public:
        FetchStaticPropNode(std::string className, std::string propName) : className(std::move(className)), propName(std::move(propName)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getClassName() const { return className; }
        const std::string &getPropName() const { return propName; }
    };

    class MethodCallNode : public ExprNode {
        ExprNode *obj;
        std::string name;
        ExprList args;

    public:
        MethodCallNode(ExprNode *obj, std::string name, ExprList args) : obj(obj), name(std::move(name)), args(std::move(args)) {}
        ~MethodCallNode() {
            delete obj;

            for (auto arg: args) {
                delete arg;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getObj() const { return obj; }
        const std::string &getName() const { return name; }
        const ExprList &getArgs() const { return args; }
    };

    class StaticMethodCallNode : public ExprNode {
        std::string className;
        std::string methodName;
        ExprList args;

    public:
        StaticMethodCallNode(std::string className, std::string methodName, ExprList args) :
                className(std::move(className)), methodName(std::move(methodName)), args(std::move(args)) {}

        ~StaticMethodCallNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getClassName() const { return className; }
        const std::string &getMethodName() const { return methodName; }
        const ExprList &getArgs() const { return args; }
    };

    class AssignPropNode : public Node {
        ExprNode *obj;
        std::string name;
        ExprNode *expr;

    public:
        AssignPropNode(ExprNode *obj, std::string name, ExprNode *expr) : obj(obj), name(std::move(name)), expr(expr) {}
        ~AssignPropNode() {
            delete obj;
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getObj() const { return obj; }
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

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getClassName() const { return className; }
        const std::string &getPropName() const { return propName; }
        ExprNode *getExpr() const { return expr; }
    };

    class NewNode : public ExprNode {
        std::string name;
        ExprList args;

    public:
        NewNode(std::string name, ExprList args) : name(std::move(name)), args(std::move(args)) {}
        ~NewNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const ExprList &getArgs() const { return args; }
    };

    class InterfaceNode : public Node {
        std::string name;
        std::vector<std::string> parents;
        StatementListNode *body;
        std::map<std::string, MethodDeclNode *> methods;

    public:
        InterfaceNode(std::string name, std::vector<std::string> parents, StatementListNode *body);
        ~InterfaceNode() {
            for (auto &[methodName, methodDeclNode]: methods) {
                delete methodDeclNode;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        const std::string &getName() const { return name; }
        const std::vector<std::string> &getParents() const { return parents; }
        StatementListNode *getBody() const { return body; }
        const std::map<std::string, MethodDeclNode *> &getMethods() const { return methods; }
        bool hasParents() const { return !parents.empty(); }
    };

    class FetchArrNode : public ExprNode {
        ExprNode *arr;
        ExprNode *idx;

    public:
        FetchArrNode(ExprNode *arr, ExprNode *idx) : arr(arr), idx(idx) {}
        ~FetchArrNode() {
            delete arr;
            delete idx;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getArr() const { return arr; }
        ExprNode *getIdx() const { return idx; }
    };

    class AssignArrNode : public Node {
        ExprNode *arr;
        ExprNode *idx;
        ExprNode *expr;

    public:
        AssignArrNode(ExprNode *arr, ExprNode *idx, ExprNode *expr) : arr(arr), idx(idx), expr(expr) {}
        ~AssignArrNode() {
            delete arr;
            delete idx;
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getArr() const { return arr; }
        ExprNode *getIdx() const { return idx; }
        ExprNode *getExpr() const { return expr; }
    };

    class AppendArrNode : public Node {
        ExprNode *arr;
        ExprNode *expr;

    public:
        AppendArrNode(ExprNode *arr, ExprNode *expr) : arr(arr), expr(expr) {}
        ~AppendArrNode() {
            delete arr;
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;

        ExprNode *getArr() const { return arr; }
        ExprNode *getExpr() const { return expr; }
    };
}

#endif //X_AST_H
