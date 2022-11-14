#include "codegen.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(FnDeclNode *node) {
        genFn(node->getName(), node->getArgs(), node->getReturnType(), nullptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnDefNode *node) {
        genFn(node->getName(), node->getArgs(), node->getReturnType(), node->getBody());

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnCallNode *node) {
        auto &name = node->getName();
        auto callee = module.getFunction(name);
        if (!callee) {
            throw CodegenException("called function is not found: " + name);
        }
        if (callee->arg_size() != node->getArgs().size()) {
            throw CodegenException("callee args mismatch");
        }

        std::vector<llvm::Value *> args;
        args.reserve(node->getArgs().size());
        for (auto arg: node->getArgs()) {
            args.push_back(arg->gen(*this));
        }

        return builder.CreateCall(callee, args);
    }

    void Codegen::genFn(
            const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body, std::optional<Type> thisType) {
        size_t paramsOffset = thisType ? 1 : 0; // this is special
        std::vector<llvm::Type *> paramTypes;
        paramTypes.reserve(args.size() + paramsOffset);
        if (thisType) {
            paramTypes.push_back(mapType(thisType.value()));
        }
        for (auto arg: args) {
            if (arg->getType().getTypeID() == Type::TypeID::VOID) {
                throw InvalidTypeException();
            }

            paramTypes.push_back(mapType(arg->getType()));
        }

        auto retType = mapType(returnType);
        auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, name, module);
        if (thisType) {
            fn->getArg(0)->setName("this");
        }
        for (auto i = 0; i < args.size(); i++) {
            fn->getArg(i + paramsOffset)->setName(args[i]->getName());
        }

        if (!body) {
            return;
        }

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        if (thisType) {
            that = fn->getArg(0);
        }

        namedValues.clear(); // todo global vars
        for (auto i = paramsOffset; i < fn->arg_size(); i++) {
            auto arg = fn->getArg(i);
            auto argName = arg->getName().str();
            auto alloca = createAlloca(arg->getType(), argName);
            builder.CreateStore(arg, alloca);
            namedValues[argName] = alloca;
        }

        body->gen(*this);
        if (!body->isLastNodeTerminate()) {
            builder.CreateRetVoid();
        }

        that = nullptr;
    }
}
