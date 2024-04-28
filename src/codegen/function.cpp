#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(FnDeclNode *node) {
        genFn(node->getName(), node->getArgs(), node->getReturnType(), nullptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnDefNode *node) {
        currentFnRetType = node->getReturnType();

        genFn(node->getName(), node->getArgs(), node->getReturnType(), node->getBody());

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnCallNode *node) {
        auto &name = node->getName();
        auto &args = node->getArgs();

        if (that) {
            try {
                return callMethod(that->value, that->type, name, args);
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

        const auto &fnType = compilerRuntime.fnTypes.at(name);

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size());
        for (auto i = 0; i < args.size(); i++) {
            auto val = args[i]->gen(*this);
            val = castTo(val, args[i]->getType(), fnType.args[i]);
            llvmArgs.push_back(val);
        }

        return builder.CreateCall(fn, llvmArgs);
    }

    void Codegen::genFn(const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body,
                        const Type *thisType) {
        size_t paramsOffset = thisType ? 1 : 0; // this is special
        // plain functions could be declared earlier
        // see declFuncs and declMethods
        llvm::Function *fn = module.getFunction(name);
        if (!fn) {
            auto fnType = genFnType(args, returnType, thisType);
            fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, name, module);
        }
        fn->setGC("x");
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
            that = Value{fn->getArg(0), *thisType};
        }

        varScopes.emplace_back();
        auto &vars = varScopes.back();

        for (auto i = 0; i < args.size(); i++) {
            auto llvmArg = fn->getArg(i + paramsOffset);
            auto &argName = args[i]->getName();
            auto &argType = args[i]->getType();

            auto alloca = createAlloca(llvmArg->getType(), argName);
            builder.CreateStore(llvmArg, alloca);
            vars[argName] = {alloca, argType};

            gcAddRoot(alloca, argType);
        }

        body->gen(*this);
        if (!body->isLastNodeTerminate()) {
            builder.CreateRetVoid();
        }

        varScopes.clear();

        that = std::nullopt;
    }

    llvm::FunctionType *Codegen::genFnType(const std::vector<ArgNode *> &args, const Type &returnType, const Type *thisType) {
        size_t paramsOffset = thisType ? 1 : 0; // this is special
        std::vector<llvm::Type *> paramTypes;
        paramTypes.reserve(args.size() + paramsOffset);
        if (thisType) {
            paramTypes.push_back(builder.getPtrTy());
        }
        for (auto arg: args) {
            paramTypes.push_back(mapType(arg->getType()));
        }

        auto retType = mapType(returnType);
        return llvm::FunctionType::get(retType, paramTypes, false);
    }

    void Codegen::checkMainFn(FnDefNode *node) const {
        if (!node->getArgs().empty()) {
            throw CodegenException("main must take no arguments");
        }

        if (!node->getReturnType().is(Type::TypeID::VOID)) {
            throw CodegenException("main must return void");
        }
    }
}
