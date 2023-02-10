#include "codegen.h"

#include "utils.h"

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
        auto &args = node->getArgs();

        if (that) {
            try {
                return callMethod(that, name, args);
            } catch (const MethodNotFoundException &e) {}
        }

        if (self) {
            try {
                return callStaticMethod(self->name, name, args);
            } catch (const MethodNotFoundException &e) {}
        }

        auto fn = module.getFunction(name);
        if (!fn) {
            throw CodegenException("called function is not found: " + name);
        }

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size());
        for (auto i = 0; i < args.size(); i++) {
            auto val = args[i]->gen(*this);
            val = castTo(val, fn->getArg(i)->getType());
            llvmArgs.push_back(val);
        }

        return builder.CreateCall(fn, llvmArgs);
    }

    void Codegen::genFn(const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body,
                        llvm::StructType *thisType) {
        size_t paramsOffset = thisType ? 1 : 0; // this is special
        // plain functions could be declared earlier
        // see declFuncs and declMethods
        llvm::Function *fn = module.getFunction(name);
        if (!fn) {
            auto fnType = genFnType(args, returnType, thisType);
            fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, name, module);
        }
        if (thisType) {
            fn->getArg(0)->setName(THIS_KEYWORD);
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

    llvm::FunctionType *Codegen::genFnType(const std::vector<ArgNode *> &args, const Type &returnType, llvm::StructType *thisType) {
        size_t paramsOffset = thisType ? 1 : 0; // this is special
        std::vector<llvm::Type *> paramTypes;
        paramTypes.reserve(args.size() + paramsOffset);
        if (thisType) {
            paramTypes.push_back(thisType->getPointerTo());
        }
        for (auto arg: args) {
            paramTypes.push_back(mapArgType(arg->getType()));
        }

        auto retType = mapType(returnType);
        return llvm::FunctionType::get(retType, paramTypes, false);
    }

    void Codegen::checkMainFn(FnDefNode *node) const {
        if (!node->getArgs().empty()) {
            throw CodegenException("main must take no arguments");
        }

        if (node->getReturnType().getTypeID() != Type::TypeID::VOID) {
            throw CodegenException("main must return void");
        }
    }
}
