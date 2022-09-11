#ifndef X_CODEGEN_H
#define X_CODEGEN_H

#include <map>
#include <stack>

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

namespace X {
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
    };

    class Codegen {
        llvm::LLVMContext &context;
        llvm::IRBuilder<> &builder;
        llvm::Module &module;
        Mangler mangler;

        std::map<std::string, llvm::AllocaInst *> namedValues;
        std::stack<Loop> loops;

        llvm::Value *that;
        ClassDecl *self; // current class in static context
        std::map<std::string, ClassDecl> classes;
        std::map<std::string, InterfaceNode *> interfaces; // todo remove from codegen

    public:
        Codegen(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module) : context(context), builder(builder), module(module) {}

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
        llvm::Value *gen(FnDeclNode *node);
        llvm::Value *gen(FnDefNode *node);
        llvm::Value *gen(FnCallNode *node);
        llvm::Value *gen(ReturnNode *node);
        llvm::Value *gen(BreakNode *node);
        llvm::Value *gen(ContinueNode *node);
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

    private:
        llvm::Type *mapType(const Type &type) const;
        llvm::Constant *getDefaultValue(const Type &type) const;
        std::pair<llvm::Type *, llvm::Value *> getVar(std::string &name) const;
        std::pair<llvm::Type *, llvm::Value *> getProp(llvm::Value *obj, const std::string &name) const;
        std::pair<llvm::Type *, llvm::Value *> getStaticProp(const std::string &className, const std::string &propName) const;
        const ClassDecl &getClass(const std::string &mangledName) const;
        llvm::AllocaInst *createAlloca(llvm::Type *type, const std::string &name) const;
        AccessModifier getMethodAccessModifier(const std::string &mangledClassName, const std::string &methodName) const;

        std::pair<llvm::Value *, llvm::Value *> upcast(llvm::Value *a, llvm::Value *b) const;
        std::pair<llvm::Value *, llvm::Value *> forceUpcast(llvm::Value *a, llvm::Value *b) const;
        llvm::Value *downcastToBool(llvm::Value *value) const;
        llvm::Type *deref(llvm::Type *type) const;

        void genFn(const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body, std::optional<Type> thisType = std::nullopt);
        void checkInterfaces(ClassNode *classNode) const; // todo remove from codegen
        bool compareDeclAndDef(MethodDeclNode *declNode, MethodDefNode *defNode) const;
    };

    class CodegenException : public std::exception {
        const char *message;

    public:
        CodegenException(const char *m) : message(m) {}
        CodegenException(const std::string &s) : message(s.c_str()) {}

        const char *what() const noexcept override {
            return message;
        }
    };
}

#endif //X_CODEGEN_H
