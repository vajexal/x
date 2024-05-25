#pragma once

#include <iostream>
#include <utility>
#include <vector>
#include <deque>
#include <unordered_map>
#include <variant>
#include <optional>
#include <string>
#include <fmt/core.h>

#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"

#include "type.h"

namespace X {
    class ExprNode;

    using ExprList = std::vector<ExprNode *>;
    using ScalarValue = std::variant<int64_t, double, bool, std::string, ExprList>;

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
        class TypeInferrer;
    }
    namespace Codegen {
        class Codegen;
    }

    class Node {
    public:
        enum class NodeKind {
            Scalar,
            StatementList,
            Unary,
            Binary,
            Decl,
            Assign,
            Var,
            If,
            While,
            For,
            Range,
            Arg,
            FnDecl,
            FnDef,
            FnCall,
            Return,
            Println,
            Break,
            Continue,
            Comment,
            PropDecl,
            MethodDecl,
            MethodDef,
            Class,
            FetchProp,
            FetchStaticProp,
            MethodCall,
            StaticMethodCall,
            AssignProp,
            AssignStaticProp,
            New,
            Interface,
            FetchArr,
            AssignArr,
            AppendArr
        };

    private:
        const NodeKind kind;

    public:
        Node(NodeKind kind) : kind(kind) {}
        virtual ~Node() = default;

        virtual void print(Pipes::PrintAst &astPrinter, int level = 0) = 0;
        virtual llvm::Value *gen(Codegen::Codegen &codegen) = 0;
        virtual Type infer(Pipes::TypeInferrer &typeInferrer) = 0;

        virtual bool isTerminate() { return false; }

        NodeKind getKind() const { return kind; }
    };

    class ExprNode : public Node {
    public:
        Type type;

        ExprNode(NodeKind kind) : Node(kind) {}
    };

    class ScalarNode : public ExprNode {
    public:
        ScalarValue value;

        ScalarNode(Type typ, ScalarValue value) : ExprNode(NodeKind::Scalar), value(std::move(value)) {
            type = std::move(typ);
        }
        ~ScalarNode() {
            if (type.is(Type::TypeID::ARRAY)) {
                for (auto expr: std::get<ExprList>(value)) {
                    delete expr;
                }
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Scalar;
        }
    };

    class StatementListNode : public Node {
    public:
        std::deque<Node *> children;

    public:
        StatementListNode() : Node(NodeKind::StatementList) {}
        ~StatementListNode() {
            for (auto child: children) {
                delete child;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        void add(Node *node) { children.push_back(node); }
        void prepend(Node *node) { children.push_front(node); }
        bool isLastNodeTerminate() const;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::StatementList;
        }
    };

    class FnDefNode;
    class ClassNode;
    class InterfaceNode;
    class DeclNode;

    class TopStatementListNode : public StatementListNode {
    public:
        std::vector<ClassNode *> classes;
        std::vector<InterfaceNode *> interfaces;
        std::vector<FnDefNode *> funcs;
        std::vector<DeclNode *> globals;

        void add(Node *node);
    };

    class UnaryNode : public ExprNode {
    public:
        OpType opType;
        ExprNode *expr;

        UnaryNode(OpType opType, ExprNode *expr) : ExprNode(NodeKind::Unary), opType(opType), expr(expr) {}
        ~UnaryNode() {
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Unary;
        }
    };

    class BinaryNode : public ExprNode {
    public:
        OpType opType;
        ExprNode *lhs;
        ExprNode *rhs;

    public:
        BinaryNode(OpType opType, ExprNode *lhs, ExprNode *rhs) : ExprNode(NodeKind::Binary), opType(opType), lhs(lhs), rhs(rhs) {}
        ~BinaryNode() {
            delete lhs;
            delete rhs;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Binary;
        }
    };

    class DeclNode : public Node {
    public:
        Type type;
        std::string name;
        ExprNode *expr;

    public:
        DeclNode(Type type, std::string name, ExprNode *expr = nullptr) :
                Node(NodeKind::Decl), type(std::move(type)), name(std::move(name)), expr(expr) {}
        ~DeclNode() {
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Decl;
        }
    };

    class AssignNode : public Node {
    public:
        std::string name;
        ExprNode *expr;

        AssignNode(std::string name, ExprNode *expr) : Node(NodeKind::Assign), name(std::move(name)), expr(expr) {}
        ~AssignNode() {
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Assign;
        }
    };

    class VarNode : public ExprNode {
    public:
        std::string name;

    public:
        VarNode(std::string name) : ExprNode(NodeKind::Var), name(std::move(name)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Var;
        }
    };

    class IfNode : public Node {
    public:
        ExprNode *cond;
        StatementListNode *thenNode;
        StatementListNode *elseNode;

    public:
        IfNode(ExprNode *cond, StatementListNode *thenNode, StatementListNode *elseNode = nullptr) :
                Node(NodeKind::If), cond(cond), thenNode(thenNode), elseNode(elseNode) {}
        ~IfNode() {
            delete cond;
            delete thenNode;
            delete elseNode;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::If;
        }
    };

    class WhileNode : public Node {
    public:
        ExprNode *cond;
        StatementListNode *body;

    public:
        WhileNode(ExprNode *cond, StatementListNode *body) : Node(NodeKind::While), cond(cond), body(body) {}
        ~WhileNode() {
            delete cond;
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::While;
        }
    };

    class ForNode : public Node {
    public:
        std::optional<std::string> idx;
        std::string val;
        ExprNode *expr;
        StatementListNode *body;

        ForNode(std::string val, ExprNode *expr, StatementListNode *body) : Node(NodeKind::For), val(std::move(val)), expr(expr), body(body) {}
        ForNode(std::string idx, std::string val, ExprNode *expr, StatementListNode *body) :
                Node(NodeKind::For), idx(std::move(idx)), val(std::move(val)), expr(expr), body(body) {}
        ~ForNode() {
            delete expr;
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::For;
        }
    };

    class RangeNode : public ExprNode {
    public:
        ExprNode *start = nullptr;
        ExprNode *stop;
        ExprNode *step = nullptr;

    public:
        RangeNode(ExprNode *stop) : ExprNode(NodeKind::Range), stop(stop) {}
        RangeNode(ExprNode *start, ExprNode *stop, ExprNode *step = nullptr) : ExprNode(NodeKind::Range), start(start), stop(stop), step(step) {}
        ~RangeNode() {
            delete start;
            delete stop;
            delete step;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Range;
        }
    };

    class ArgNode : public Node {
    public:
        Type type;
        std::string name;

    public:
        ArgNode(Type type, std::string name) : Node(NodeKind::Arg), type(std::move(type)), name(std::move(name)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool operator==(const ArgNode &other) const;
        bool operator!=(const ArgNode &other) const;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Arg;
        }
    };

    class FnDeclNode : public Node {
    public:
        std::string name;
        std::vector<ArgNode *> args;
        Type returnType;

    public:
        FnDeclNode(std::string name, std::vector<ArgNode *> args, Type returnType) :
                Node(NodeKind::FnDecl), name(std::move(name)), args(std::move(args)), returnType(std::move(returnType)) {}
        ~FnDeclNode() {
            for (auto arg: args) {
                delete arg;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool operator==(const FnDeclNode &other) const;
        bool operator!=(const FnDeclNode &other) const;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::FnDecl;
        }
    };

    class FnDefNode : public Node {
    public:
        FnDeclNode *decl;
        StatementListNode *body;

    public:
        FnDefNode(FnDeclNode *decl, StatementListNode *body) : Node(NodeKind::FnDef), decl(decl), body(body) {}
        ~FnDefNode() {
            delete decl;
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool operator==(const FnDefNode &other) const;
        bool operator!=(const FnDefNode &other) const;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::FnDef;
        }
    };

    class FnCallNode : public ExprNode {
    public:
        std::string name;
        ExprList args;

    public:
        FnCallNode(std::string name, ExprList args) : ExprNode(NodeKind::FnCall), name(std::move(name)), args(std::move(args)) {}
        ~FnCallNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::FnCall;
        }
    };

    class ReturnNode : public Node {
    public:
        ExprNode *val;

    public:
        explicit ReturnNode(ExprNode *val = nullptr) : Node(NodeKind::Return), val(val) {}
        ~ReturnNode() {
            delete val;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool isTerminate() override { return true; };

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Return;
        }
    };

    class PrintlnNode : public Node {
    public:
        ExprNode *val;

    public:
        explicit PrintlnNode(ExprNode *val) : Node(NodeKind::Println), val(val) {}
        ~PrintlnNode() {
            delete val;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Println;
        }
    };

    class BreakNode : public Node {
    public:
        BreakNode() : Node(NodeKind::Break) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool isTerminate() override { return true; };

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Break;
        }
    };

    class ContinueNode : public Node {
    public:
        ContinueNode() : Node(NodeKind::Continue) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool isTerminate() override { return true; };

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Continue;
        }
    };

    class CommentNode : public Node {
    public:
        std::string comment;

        CommentNode(std::string comment) : Node(NodeKind::Comment), comment(std::move(comment)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Comment;
        }
    };

    class PropDeclNode : public Node {
    public:
        DeclNode *decl;
        AccessModifier accessModifier;
        bool isStatic;

        PropDeclNode(DeclNode *decl, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) :
                Node(NodeKind::PropDecl), decl(decl), accessModifier(accessModifier), isStatic(isStatic) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::PropDecl;
        }
    };

    class MethodDeclNode : public Node {
    public:
        FnDeclNode *fnDecl;
        bool isAbstract;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        MethodDeclNode(FnDeclNode *fnDecl, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) :
                Node(NodeKind::MethodDecl), fnDecl(fnDecl), accessModifier(accessModifier), isStatic(isStatic) {}
        ~MethodDeclNode() {
            delete fnDecl;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool operator==(const MethodDeclNode &other) const;
        bool operator!=(const MethodDeclNode &other) const;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::MethodDecl;
        }
    };

    class MethodDefNode : public Node {
    public:
        FnDefNode *fnDef;
        AccessModifier accessModifier;
        bool isStatic;

    public:
        MethodDefNode(FnDefNode *fnDef, AccessModifier accessModifier = AccessModifier::PUBLIC, bool isStatic = false) :
                Node(NodeKind::MethodDef), fnDef(fnDef), accessModifier(accessModifier), isStatic(isStatic) {}
        ~MethodDefNode() {
            delete fnDef;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool operator==(const MethodDefNode &other) const;
        bool operator!=(const MethodDefNode &other) const;
        bool operator==(const MethodDeclNode &decl) const;
        bool operator!=(const MethodDeclNode &decl) const;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::MethodDef;
        }
    };

    class ClassNode : public Node {
    public:
        std::string name;
        StatementListNode *body;
        std::vector<PropDeclNode *> props;
        std::unordered_map<std::string, MethodDefNode *> methods;
        std::unordered_map<std::string, MethodDeclNode *> abstractMethods;
        std::string parent;
        std::vector<std::string> interfaces;
        bool abstract;

        ClassNode(std::string name, StatementListNode *body, std::string parent, std::vector<std::string> interfaces, bool abstract);
        ~ClassNode() {
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool hasParent() const { return !parent.empty(); }

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Class;
        }
    };

    class FetchPropNode : public ExprNode {
    public:
        ExprNode *obj;
        std::string name;

        FetchPropNode(ExprNode *obj, std::string name) : ExprNode(NodeKind::FetchProp), obj(obj), name(std::move(name)) {}
        ~FetchPropNode() {
            delete obj;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::FetchProp;
        }
    };

    class FetchStaticPropNode : public ExprNode {
    public:
        std::string className;
        std::string propName;

        FetchStaticPropNode(std::string className, std::string propName) :
                ExprNode(NodeKind::FetchStaticProp), className(std::move(className)), propName(std::move(propName)) {}

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::FetchStaticProp;
        }
    };

    class MethodCallNode : public ExprNode {
    public:
        ExprNode *obj;
        std::string name;
        ExprList args;

        MethodCallNode(ExprNode *obj, std::string name, ExprList args) :
                ExprNode(NodeKind::MethodCall), obj(obj), name(std::move(name)), args(std::move(args)) {}
        ~MethodCallNode() {
            delete obj;

            for (auto arg: args) {
                delete arg;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::MethodCall;
        }
    };

    class StaticMethodCallNode : public ExprNode {
    public:
        std::string className;
        std::string methodName;
        ExprList args;

        StaticMethodCallNode(std::string className, std::string methodName, ExprList args) :
                ExprNode(NodeKind::StaticMethodCall), className(std::move(className)), methodName(std::move(methodName)), args(std::move(args)) {}

        ~StaticMethodCallNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::StaticMethodCall;
        }
    };

    class AssignPropNode : public Node {
    public:
        ExprNode *obj;
        std::string name;
        ExprNode *expr;

    public:
        AssignPropNode(ExprNode *obj, std::string name, ExprNode *expr) : Node(NodeKind::AssignProp), obj(obj), name(std::move(name)), expr(expr) {}
        ~AssignPropNode() {
            delete obj;
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::AssignProp;
        }
    };

    class AssignStaticPropNode : public Node {
    public:
        std::string className;
        std::string propName;
        ExprNode *expr;

    public:
        AssignStaticPropNode(std::string className, std::string propName, ExprNode *expr) :
                Node(NodeKind::AssignStaticProp), className(std::move(className)), propName(std::move(propName)), expr(expr) {}

        ~AssignStaticPropNode() {
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::AssignStaticProp;
        }
    };

    class NewNode : public ExprNode {
    public:
        std::string name;
        ExprList args;

    public:
        NewNode(std::string name, ExprList args) : ExprNode(NodeKind::New), name(std::move(name)), args(std::move(args)) {}
        ~NewNode() {
            for (auto expr: args) {
                delete expr;
            }
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::New;
        }
    };

    class InterfaceNode : public Node {
    public:
        std::string name;
        std::vector<std::string> parents;
        StatementListNode *body;
        std::unordered_map<std::string, MethodDeclNode *> methods;

    public:
        InterfaceNode(std::string name, std::vector<std::string> parents, StatementListNode *body);
        ~InterfaceNode() {
            delete body;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        bool hasParents() const { return !parents.empty(); }

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::Interface;
        }
    };

    class FetchArrNode : public ExprNode {
    public:
        ExprNode *arr;
        ExprNode *idx;

    public:
        FetchArrNode(ExprNode *arr, ExprNode *idx) : ExprNode(NodeKind::FetchArr), arr(arr), idx(idx) {}
        ~FetchArrNode() {
            delete arr;
            delete idx;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::FetchArr;
        }
    };

    class AssignArrNode : public Node {
    public:
        ExprNode *arr;
        ExprNode *idx;
        ExprNode *expr;

    public:
        AssignArrNode(ExprNode *arr, ExprNode *idx, ExprNode *expr) : Node(NodeKind::AssignArr), arr(arr), idx(idx), expr(expr) {}
        ~AssignArrNode() {
            delete arr;
            delete idx;
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::AssignArr;
        }
    };

    class AppendArrNode : public Node {
    public:
        ExprNode *arr;
        ExprNode *expr;

    public:
        AppendArrNode(ExprNode *arr, ExprNode *expr) : Node(NodeKind::AppendArr), arr(arr), expr(expr) {}
        ~AppendArrNode() {
            delete arr;
            delete expr;
        }

        void print(Pipes::PrintAst &astPrinter, int level = 0) override;
        llvm::Value *gen(Codegen::Codegen &codegen) override;
        Type infer(Pipes::TypeInferrer &typeInferrer) override;

        static bool classof(const Node *node) {
            return node->getKind() == NodeKind::AppendArr;
        }
    };
}
