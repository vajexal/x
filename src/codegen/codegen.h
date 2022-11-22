#ifndef X_CODEGEN_H
#define X_CODEGEN_H

#include <map>
#include <stack>
#include <vector>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constant.h"

#include "ast.h"
#include "mangler.h"
#include "runtime/runtime.h"

namespace X::Codegen {
    struct Loop {
        llvm::BasicBlock *start;
        llvm::BasicBlock *end;

        Loop(llvm::BasicBlock *start, llvm::BasicBlock *end) : start(start), end(end) {}
    };

    struct Prop {
        llvm::Type *type;
        uint64_t pos;
        AccessModifier accessModifier;
    };

    struct StaticProp {
        llvm::GlobalVariable *var;
        AccessModifier accessModifier;
    };

    struct Method {
        AccessModifier accessModifier;
    };

    struct ClassDecl {
        llvm::StructType *type;
        std::map<std::string, Prop> props;
        std::map<std::string, StaticProp> staticProps;
        std::map<std::string, Method> methods;
        ClassDecl *parent = nullptr;
        bool isAbstract;
    };

    class Codegen {
        static inline const std::string CONSTRUCTOR_FN_NAME = "construct";
        static inline const int INTEGER_BIT_WIDTH = 64;

        llvm::LLVMContext &context;
        llvm::IRBuilder<> &builder;
        llvm::Module &module;
        Mangler mangler;
        Runtime::ArrayRuntime arrayRuntime;

        std::map<std::string, llvm::AllocaInst *> namedValues;
        std::stack<Loop> loops;

        llvm::Value *that = nullptr;
        ClassDecl *self = nullptr; // current class in static context
        std::map<std::string, ClassDecl> classes;

    public:
        Codegen(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module) :
                context(context), builder(builder), module(module), arrayRuntime(Runtime::ArrayRuntime(context, module)) {}

        llvm::Value *gen(Node *node);
        llvm::Value *gen(StatementListNode *node);
        llvm::Value *gen(ScalarNode *node);
        llvm::Value *gen(UnaryNode *node);
        llvm::Value *gen(BinaryNode *node);
        llvm::Value *gen(DeclareNode *node);
        llvm::Value *gen(AssignNode *node);
        llvm::Value *gen(VarNode *node);
        llvm::Value *gen(IfNode *node);
        llvm::Value *gen(WhileNode *node);
        llvm::Value *gen(ForNode *node);
        llvm::Value *gen(RangeNode *node);
        llvm::Value *gen(BreakNode *node);
        llvm::Value *gen(ContinueNode *node);
        llvm::Value *gen(FnDeclNode *node);
        llvm::Value *gen(FnDefNode *node);
        llvm::Value *gen(FnCallNode *node);
        llvm::Value *gen(ReturnNode *node);
        llvm::Value *gen(PrintlnNode *node);
        llvm::Value *gen(CommentNode *node);
        llvm::Value *gen(ClassNode *node);
        llvm::Value *gen(FetchPropNode *node);
        llvm::Value *gen(FetchStaticPropNode *node);
        llvm::Value *gen(MethodCallNode *node);
        llvm::Value *gen(StaticMethodCallNode *node);
        llvm::Value *gen(AssignPropNode *node);
        llvm::Value *gen(AssignStaticPropNode *node);
        llvm::Value *gen(NewNode *node);
        llvm::Value *gen(InterfaceNode *node);
        llvm::Value *gen(FetchArrNode *node);
        llvm::Value *gen(AssignArrNode *node);
        llvm::Value *gen(AppendArrNode *node);

    private:
        llvm::Type *mapType(const Type &type);
        llvm::Constant *getDefaultValue(const Type &type);
        /// differs from getDefaultValue because getDefaultValue returns constant and createDefaultValue can emit instructions
        llvm::Value *createDefaultValue(const Type &type);
        std::pair<llvm::Type *, llvm::Value *> getVar(std::string &name) const;
        std::pair<llvm::Type *, llvm::Value *> getProp(llvm::Value *obj, const std::string &name) const;
        std::pair<llvm::Type *, llvm::Value *> getStaticProp(const std::string &className, const std::string &propName) const;
        const ClassDecl &getClass(const std::string &mangledName) const;
        bool isObject(llvm::Value *value) const;
        llvm::AllocaInst *createAlloca(llvm::Type *type, const std::string &name = "") const;
        AccessModifier getMethodAccessModifier(const std::string &mangledClassName, const std::string &methodName) const;
        llvm::Function *getConstructor(const std::string &mangledClassName) const;
        void checkConstructor(MethodDefNode *node, const std::string &className) const;
        llvm::Value *callMethod(llvm::Value *obj, const std::string &methodName, const std::vector<ExprNode *> &args);

        std::pair<llvm::Value *, llvm::Value *> upcast(llvm::Value *a, llvm::Value *b) const;
        std::pair<llvm::Value *, llvm::Value *> forceUpcast(llvm::Value *a, llvm::Value *b) const;
        llvm::Value *downcastToBool(llvm::Value *value) const;
        llvm::Value *castToString(llvm::Value *value) const;

        void genFn(const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body,
                   std::optional<Type> thisType = std::nullopt);
        std::pair<llvm::Function *, llvm::Type *> findMethod(llvm::StructType *type, const std::string &methodName) const;
        llvm::Value *compareStrings(llvm::Value *first, llvm::Value *second) const;
        llvm::Value *negate(llvm::Value *value) const;

        llvm::StructType *getArrayForType(const Type *type);
        void fillArray(llvm::Value *arr, const std::vector<llvm::Value *> &values);
    };

    class CodegenException : public std::exception {
        std::string message;

    public:
        CodegenException(const char *m) : message(m) {}
        CodegenException(std::string s) : message(std::move(s)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };

    class VarNotFoundException : public CodegenException {
    public:
        VarNotFoundException(const std::string &varName) : CodegenException("var not found: " + varName) {}
    };

    class VarAlreadyExistsException : public CodegenException {
    public:
        VarAlreadyExistsException(const std::string &varName) : CodegenException("var already exists: " + varName) {}
    };

    class MethodNotFoundException : public CodegenException {
    public:
        MethodNotFoundException(const std::string &methodName) : CodegenException("method not found: " + methodName) {}
    };

    class InvalidObjectAccessException : public CodegenException {
    public:
        InvalidObjectAccessException() : CodegenException("invalid object exception") {}
    };

    class InvalidArrayAccessException : public CodegenException {
    public:
        InvalidArrayAccessException() : CodegenException("invalid array exception") {}
    };

    class InvalidOpTypeException : public CodegenException {
    public:
        InvalidOpTypeException() : CodegenException("invalid operation type") {}
    };

    class InvalidTypeException : public CodegenException {
    public:
        InvalidTypeException() : CodegenException("invalid type") {}
    };
}

#endif //X_CODEGEN_H
