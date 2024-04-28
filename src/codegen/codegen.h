#pragma once

#include <unordered_map>
#include <unordered_map>
#include <stack>
#include <utility>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <deque>
#include <fmt/core.h>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constant.h"

#include "ast.h"
#include "compiler_runtime.h"
#include "mangler.h"
#include "runtime/runtime.h"
#include "gc/gc.h"

namespace X::Codegen {
    struct Value {
        llvm::Value *value;
        Type type;
    };

    struct Loop {
        llvm::BasicBlock *start;
        llvm::BasicBlock *end;

        Loop(llvm::BasicBlock *start, llvm::BasicBlock *end) : start(start), end(end) {}
    };

    struct Prop {
        Type type;
        uint64_t pos;
        AccessModifier accessModifier;

        Prop(Type type, uint64_t pos, AccessModifier accessModifier) : type(std::move(type)), pos(pos), accessModifier(accessModifier) {}
    };

    struct StaticProp {
        llvm::GlobalVariable *var;
        Type type;
        AccessModifier accessModifier;
    };

    struct Method {
        AccessModifier accessModifier;
        llvm::FunctionType *type;
        bool isVirtual;
        uint64_t vtablePos;
    };

    struct ClassDecl {
        std::string name;
        Type type;
        llvm::StructType *llvmType;
        std::unordered_map<std::string, Prop> props;
        std::unordered_map<std::string, StaticProp> staticProps;
        std::unordered_map<std::string, Method> methods;
        ClassDecl *parent = nullptr;
        bool isAbstract = false;
        llvm::StructType *vtableType = nullptr;
        GC::Metadata *meta;
    };

    struct InterfaceDecl {
        std::string name;
        Type type;
        llvm::StructType *llvmType;
        std::unordered_map<std::string, Method> methods;
        llvm::StructType *vtableType = nullptr;
    };

    class Codegen {
        llvm::LLVMContext &context;
        llvm::IRBuilder<> &builder;
        llvm::Module &module;
        CompilerRuntime &compilerRuntime;
        Runtime::ArrayRuntime arrayRuntime;
        GC::GC &gc;

        std::deque<std::unordered_map<std::string, Value>> varScopes;
        std::stack<Loop> loops;

        Type currentFnRetType;
        std::optional<Value> that;
        ClassDecl *self = nullptr; // current class in static context
        std::unordered_map<std::string, ClassDecl> classes;
        std::unordered_map<std::string, InterfaceDecl> interfaces;
        std::unordered_set<std::string> symbols;

        std::unordered_map<std::string, GC::Metadata *> gcMetaCache;

    public:
        static inline const std::string MAIN_FN_NAME = "main";

        Codegen(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module, CompilerRuntime &compilerRuntime, GC::GC &gc) :
                context(context), builder(builder), module(module), compilerRuntime(compilerRuntime), arrayRuntime(Runtime::ArrayRuntime(context, module)),
                gc(gc) {}

        void genProgram(TopStatementListNode *node);

        llvm::Value *gen(Node *node);
        llvm::Value *gen(StatementListNode *node);
        llvm::Value *gen(TopStatementListNode *node);
        llvm::Value *gen(ScalarNode *node);
        llvm::Value *gen(UnaryNode *node);
        llvm::Value *gen(BinaryNode *node);
        llvm::Value *gen(DeclNode *node);
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
        void declInterfaces(TopStatementListNode *node);
        void declClasses(TopStatementListNode *node);
        void declMethods(TopStatementListNode *node);
        void declProps(TopStatementListNode *node);
        void declFuncs(TopStatementListNode *node);

        llvm::Type *mapType(const Type &type);
        llvm::Constant *getDefaultValue(const Type &type);
        /// differs from getDefaultValue because getDefaultValue returns constant and createDefaultValue can emit instructions
        llvm::Value *createDefaultValue(const Type &type);
        std::pair<Type, llvm::Value *> getVar(std::string &name);
        std::pair<const Type &, llvm::Value *> getProp(llvm::Value *obj, const Type &objType, const std::string &name);
        std::pair<const Type &, llvm::Value *> getStaticProp(const std::string &className, const std::string &propName) const;
        const ClassDecl &getClassDecl(const std::string &name) const;
        const InterfaceDecl *findInterfaceDecl(const std::string &name) const;
        std::string getClassName(const Type &type) const;
        GC::Metadata *getTypeGCMeta(const Type &type);
        GC::Metadata *genTypeGCMeta(const Type &type);
        static std::string getTypeGCMetaKey(const Type &type);
        llvm::Value *getGCMetaValue(const Type &type);
        bool isObject(const Type &type) const;
        bool isInterfaceType(const Type &type) const;
        llvm::AllocaInst *createAlloca(llvm::Type *type, const std::string &name = "") const;
        // get constructor of internal class (String, Array, ...)
        llvm::Function *getInternalConstructor(const std::string &mangledClassName) const;
        void checkConstructor(MethodDefNode *node, const std::string &className) const;
        llvm::Value *callMethod(llvm::Value *obj, const Type &objType, const std::string &methodName, const ExprList &args);
        llvm::Value *callStaticMethod(const std::string &className, const std::string &methodName, const ExprList &args);
        llvm::Value *newObj(llvm::StructType *type);
        llvm::StructType *genVtable(ClassNode *classNode, ClassDecl &classDecl);
        llvm::StructType *genVtable(InterfaceNode *classNode, InterfaceDecl &interfaceDecl);
        void initVtable(llvm::Value *obj, const ClassDecl &classDecl);
        void initInterfaceVtable(llvm::Value *obj, const Type &objType, llvm::Value *interface, const InterfaceDecl &interfaceDecl);

        std::tuple<llvm::Value *, Type, llvm::Value *, Type> upcast(llvm::Value *a, Type aType, llvm::Value *b, Type bType) const;
        std::tuple<llvm::Value *, Type, llvm::Value *, Type> forceUpcast(llvm::Value *a, Type aType, llvm::Value *b, Type bType) const;
        llvm::Value *downcastToBool(llvm::Value *value, const Type &type) const;
        llvm::Value *castToString(llvm::Value *value, const Type &type) const;
        bool instanceof(const Type &instanceType, const Type &type) const;
        llvm::Value *castTo(llvm::Value *value, const Type &type, const Type &expectedType);
        llvm::Value *instantiateInterface(llvm::Value *value, const Type &type, const InterfaceDecl &interfaceDecl);

        llvm::Value *genLogicalAnd(BinaryNode *node);
        llvm::Value *genLogicalOr(BinaryNode *node);

        void genFn(const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body,
                   const Type *thisType = nullptr);
        llvm::FunctionType *genFnType(const std::vector<ArgNode *> &args, const Type &returnType, const Type *thisType = nullptr);
        void checkMainFn(FnDefNode *node) const;
        std::tuple<llvm::FunctionCallee, FnType *> findMethod(llvm::Value *obj, const Type &objType, const std::string &methodName);
        // find class method only by looking for generated funcs (ignoring internal classes, virtual funcs, access modifiers etc.)
        llvm::Function *simpleFindMethod(const ClassDecl &classDecl, const std::string &methodName) const;
        llvm::Value *compareStrings(llvm::Value *first, llvm::Value *second) const;
        llvm::Value *negate(llvm::Value *value) const;

        llvm::StructType *getArrayForType(const Type &subtype);
        void fillArray(llvm::Value *arr, const Type &type, const std::vector<llvm::Value *> &values);

        void addSymbol(const std::string &symbol);

        // gc helpers
        llvm::Value *gcAlloc(llvm::Value *size);
        void gcAddRoot(llvm::Value *root, const Type &type);
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
        VarNotFoundException(const std::string &varName) : CodegenException(fmt::format("var {} not found", varName)) {}
    };

    class VarAlreadyExistsException : public CodegenException {
    public:
        VarAlreadyExistsException(const std::string &varName) : CodegenException(fmt::format("var {} already exists", varName)) {}
    };

    class FnAlreadyDeclaredException : public CodegenException {
    public:
        FnAlreadyDeclaredException(const std::string &fnName) : CodegenException(fmt::format("function {} already declared", fnName)) {}
    };

    class ClassNotFoundException : public CodegenException {
    public:
        ClassNotFoundException(const std::string &className) : CodegenException(fmt::format("class {} not found", className)) {}
    };

    class ClassAlreadyExistsException : public CodegenException {
    public:
        ClassAlreadyExistsException(const std::string &className) : CodegenException(fmt::format("class {} already exists", className)) {}
    };

    class PropNotFoundException : public CodegenException {
    public:
        PropNotFoundException(const std::string &className, const std::string &propName) :
                CodegenException(fmt::format("property {}::{} not found", className, propName)) {}
    };

    class PropAlreadyDeclaredException : public CodegenException {
    public:
        PropAlreadyDeclaredException(const std::string &className, const std::string &propName) :
                CodegenException(fmt::format("property {}::{} already declared", className, propName)) {}
    };

    class PropAccessException : public CodegenException {
    public:
        PropAccessException(const std::string &className, const std::string &propName) :
                CodegenException(fmt::format("cannot access private property {}::{}", className, propName)) {} // todo access modifier
    };

    class MethodNotFoundException : public CodegenException {
    public:
        MethodNotFoundException(const std::string &className, const std::string &methodName) :
                CodegenException(fmt::format("method {}::{} not found", className, methodName)) {}
    };

    class MethodAlreadyDeclaredException : public CodegenException {
    public:
        MethodAlreadyDeclaredException(const std::string &className, const std::string &methodName) :
                CodegenException(fmt::format("method {}::{} already declared", className, methodName)) {}
    };

    class MethodAccessException : public CodegenException {
    public:
        MethodAccessException(const std::string &className, const std::string &methodName) :
                CodegenException(fmt::format("cannot access private method {}::{}", className, methodName)) {} // todo access modifier
    };

    class SymbolAlreadyExistsException : public CodegenException {
    public:
        SymbolAlreadyExistsException(const std::string &symbol) : CodegenException(fmt::format("symbol {} already exists", symbol)) {}
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

    class BinaryArgIsEmptyException : public CodegenException {
    public:
        BinaryArgIsEmptyException() : CodegenException("binary arg is empty") {}
    };
}
