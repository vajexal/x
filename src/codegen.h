#ifndef X_CODEGEN_H
#define X_CODEGEN_H

#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"

#include "ast.h"

namespace X {
    class Codegen {
        llvm::LLVMContext &context;
        llvm::IRBuilder<> &builder;
        llvm::Module &module;
        std::map<std::string, llvm::AllocaInst *> namedValues;
        llvm::BasicBlock *endBB;
        llvm::AllocaInst *retval;

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
        llvm::Value *gen(FnNode *node);
        llvm::Value *gen(FnCallNode *node);
        llvm::Value *gen(ReturnNode *node);
        llvm::Value *gen(BreakNode *node);
        llvm::Value *gen(ContinueNode *node);
        llvm::Value *gen(CommentNode *node);

    private:
        llvm::Type *mapType(Type type);
        llvm::AllocaInst *getVar(std::string &name);

        std::pair<llvm::Value *, llvm::Value *> upcast(llvm::Value *a, llvm::Value *b);
        std::pair<llvm::Value *, llvm::Value *> forceUpcast(llvm::Value *a, llvm::Value *b);
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
