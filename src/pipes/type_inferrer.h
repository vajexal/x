#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <optional>
#include <set>

#include "pipeline.h"
#include "compiler_runtime.h"
#include "ast.h"

namespace X::Pipes {
    struct FnType {
        std::vector<Type> args;
        Type retType;
    };

    struct PropType {
        Type type;
        bool isStatic;
    };

    struct MethodType : FnType {
        bool isStatic;
    };

    class TypeInferrer : public Pipe {
        CompilerRuntime &compilerRuntime;

        std::deque<std::unordered_map<std::string, Type>> varScopes;
        std::map<std::string, FnType> fnTypes;
        // name of the current class (empty string if not in class scope)
        std::optional<std::string> self;
        std::optional<std::string> that;
        Type currentFnRetType;
        // class name -> {prop name -> type}
        std::map<std::string, std::map<std::string, PropType>> classProps;
        // class name -> {method name -> return type}
        std::map<std::string, std::map<std::string, MethodType>> classMethodTypes;
        std::set<std::string> classes;

    public:
        explicit TypeInferrer(CompilerRuntime &compilerRuntime) : compilerRuntime(compilerRuntime) {}

        TopStatementListNode *handle(TopStatementListNode *node) override;

        // todo a lot of copying, maybe make unique types, like in llvm
        Type infer(Node *node);
        Type infer(StatementListNode *node);
        Type infer(ScalarNode *node);
        Type infer(UnaryNode *node);
        Type infer(BinaryNode *node);
        Type infer(DeclNode *node);
        Type infer(AssignNode *node);
        Type infer(VarNode *node);
        Type infer(IfNode *node);
        Type infer(WhileNode *node);
        Type infer(ForNode *node);
        Type infer(RangeNode *node);
        Type infer(BreakNode *node);
        Type infer(ContinueNode *node);
        Type infer(ArgNode *node);
        Type infer(FnDeclNode *node);
        Type infer(FnDefNode *node);
        Type infer(FnCallNode *node);
        Type infer(ReturnNode *node);
        Type infer(PrintlnNode *node);
        Type infer(CommentNode *node);
        Type infer(ClassNode *node);
        Type infer(MethodDefNode *node);
        Type infer(FetchPropNode *node);
        Type infer(FetchStaticPropNode *node);
        Type infer(MethodCallNode *node);
        Type infer(StaticMethodCallNode *node);
        Type infer(AssignPropNode *node);
        Type infer(AssignStaticPropNode *node);
        Type infer(NewNode *node);
        Type infer(MethodDeclNode *node);
        Type infer(InterfaceNode *node);
        Type infer(FetchArrNode *node);
        Type infer(AssignArrNode *node);
        Type infer(AppendArrNode *node);

    private:
        void addRuntime();
        void declClasses(TopStatementListNode *node);
        void declMethods(TopStatementListNode *node);
        void declFuncs(TopStatementListNode *node);

        void checkIfTypeIsValid(const Type &type) const;
        void checkIfLvalueTypeIsValid(const Type &type) const;
        void checkFnCall(const FnType &fnType, const ExprList &args);
        const Type &getMethodReturnType(FnDeclNode *fnDecl, const std::string &className) const;
        const Type &getMethodReturnType(FnDefNode *fnDef, const std::string &className) const;
        Type getVarType(const std::string &name) const;
        const FnType &getFnType(const std::string &fnName) const;
        const MethodType &getMethodType(const std::string &className, const std::string &methodName, bool isStatic = false) const;
        const Type &getPropType(const std::string &className, const std::string &propName, bool isStatic = false) const;
        std::string getObjectClassName(const Type &objType) const;
        bool canCastTo(const Type &type, const Type &expectedType) const;
        bool instanceof(const Type &instanceType, const Type &type) const;
    };

    class TypeInferrerException : public std::exception {
        std::string message;

    public:
        TypeInferrerException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };

    class InvalidTypeException : public TypeInferrerException {
    public:
        InvalidTypeException() : TypeInferrerException("invalid type") {}
    };
}
