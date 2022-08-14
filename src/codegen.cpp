#include "codegen.h"

namespace X {
    llvm::Value *Codegen::gen(Node *node) {
        throw CodegenException("can't gen node");
    }

    llvm::Value *Codegen::gen(StatementListNode *node) {
        for (auto &child: node->getChildren()) {
            child->gen(*this);
        }

        return nullptr;
    }

    llvm::Value *Codegen::gen(ScalarNode *node) {
        auto value = node->getValue();

        switch (node->getType()) {
            case Type::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), std::get<int>(value));
            case Type::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getFloatTy(context), std::get<float>(value));
            case Type::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), std::get<bool>(value));
            default:
                throw CodegenException("invalid scalar type");
        }
    }

    llvm::Value *Codegen::gen(UnaryNode *node) {
        auto expr = node->getExpr()->gen(*this);

        switch (node->getType()) {
            case OpType::POST_INC: {
                auto value = builder.CreateAdd(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
                auto name = dynamic_cast<VarNode *>(node->getExpr())->getName(); // todo
                auto var = getVar(name);
                builder.CreateStore(value, var);
                return expr;
            }
            case OpType::POST_DEC: {
                auto value = builder.CreateSub(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
                auto name = dynamic_cast<VarNode *>(node->getExpr())->getName(); // todo
                auto var = getVar(name);
                builder.CreateStore(value, var);
                return expr;
            }
            case OpType::NOT:
                // todo true binary op
                return builder.CreateXor(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
            default:
                throw CodegenException("invalid op type");
        }
    }

    llvm::Value *Codegen::gen(BinaryNode *node) {
        auto lhs = node->getLhs()->gen(*this);
        auto rhs = node->getRhs()->gen(*this);

        if (!lhs || !rhs) {
            throw CodegenException("binary arg is empty");
        }

        switch (node->getType()) {
            case OpType::PLUS:
                return builder.CreateAdd(lhs, rhs);
            case OpType::MINUS:
                return builder.CreateSub(lhs, rhs);
            case OpType::MUL:
                return builder.CreateMul(lhs, rhs);
            case OpType::DIV:
                if (lhs->getType()->isIntegerTy()) {
                    lhs = builder.CreateSIToFP(lhs, llvm::Type::getFloatTy(context));
                }
                if (rhs->getType()->isIntegerTy()) {
                    rhs = builder.CreateSIToFP(rhs, llvm::Type::getFloatTy(context));
                }
                return builder.CreateFDiv(lhs, rhs);
            case OpType::POW:
                return nullptr;
            case OpType::OR:
                return nullptr;
            case OpType::AND:
                return nullptr;
            case OpType::EQUAL:
                return builder.CreateICmpEQ(lhs, rhs);
            case OpType::NOT_EQUAL:
                return builder.CreateICmpNE(lhs, rhs);
            case OpType::SMALLER:
                return builder.CreateICmpSLT(lhs, rhs);
            case OpType::SMALLER_OR_EQUAL:
                return builder.CreateICmpSLE(lhs, rhs);
            case OpType::GREATER:
                return builder.CreateICmpSGT(lhs, rhs);
            case OpType::GREATER_OR_EQUAL:
                return builder.CreateICmpSGE(lhs, rhs);
            default:
                throw CodegenException("invalid op type");
        }
    }

    llvm::Value *Codegen::gen(DeclareNode *node) {
        auto type = mapType(node->getType());
        auto name = node->getName();
        auto stackVar = builder.CreateAlloca(type, nullptr, name);
        auto value = node->getExpr()->gen(*this);
        builder.CreateStore(value, stackVar);
        namedValues[name] = stackVar;
        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignNode *node) {
        auto name = node->getName();
        auto var = getVar(name);
        auto value = node->getExpr()->gen(*this);
        builder.CreateStore(value, var);
        return nullptr;
    }

    llvm::Value *Codegen::gen(VarNode *node) {
        auto name = node->getName();
        auto var = getVar(name);
        return builder.CreateLoad(var->getAllocatedType(), var, name);
    }

    llvm::Value *Codegen::gen(IfNode *node) {
        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("if cond is empty");
        }
        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto thenBB = llvm::BasicBlock::Create(context, "then", parentFunction);
        auto elseBB = llvm::BasicBlock::Create(context, "else");
        auto mergeBB = llvm::BasicBlock::Create(context, "ifcont");

        auto thenNode = node->getThenNode();
        auto elseNode = node->getElseNode();

        if (elseNode) {
            builder.CreateCondBr(cond, thenBB, elseBB);
        } else {
            builder.CreateCondBr(cond, thenBB, mergeBB);
        }

        builder.SetInsertPoint(thenBB);
        thenNode->gen(*this);
        if (!thenNode->isLastNodeTerminate()) {
            builder.CreateBr(mergeBB);
        }

        if (elseNode) {
            parentFunction->getBasicBlockList().push_back(elseBB);
            builder.SetInsertPoint(elseBB);
            elseNode->gen(*this);
            if (!elseNode->isLastNodeTerminate()) {
                builder.CreateBr(mergeBB);
            }
        }

        parentFunction->getBasicBlockList().push_back(mergeBB);
        builder.SetInsertPoint(mergeBB);

        return nullptr;
    }

    llvm::Value *Codegen::gen(WhileNode *node) {
        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto loopStartBB = llvm::BasicBlock::Create(context, "loopStart");
        auto loopBB = llvm::BasicBlock::Create(context, "loop");
        auto loopEndBB = llvm::BasicBlock::Create(context, "loopEnd");

        builder.CreateBr(loopStartBB);
        parentFunction->getBasicBlockList().push_back(loopStartBB);
        builder.SetInsertPoint(loopStartBB);

        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("while cond is empty");
        }
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        parentFunction->getBasicBlockList().push_back(loopBB);
        builder.SetInsertPoint(loopBB);
        node->getBody()->gen(*this);
        builder.CreateBr(loopStartBB);

        parentFunction->getBasicBlockList().push_back(loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnNode *node) {
        std::vector<llvm::Type *> paramTypes;
        auto args = node->getArgs();
        for (auto &arg: args) {
            auto argType = mapType(arg->getType());
            paramTypes.push_back(argType);
        }
        auto retType = mapType(node->getReturnType());
        auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, node->getName(), module);
        for (auto i = 0; i < fn->arg_size(); i++) {
            fn->getArg(i)->setName(args[i]->getName());
        }

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        namedValues.clear();
        for (auto &arg: fn->args()) {
            auto alloca = builder.CreateAlloca(arg.getType(), nullptr, arg.getName());
            builder.CreateStore(&arg, alloca);
            namedValues[arg.getName().str()] = alloca;
        }

        if (node->getReturnType() != Type::VOID) {
            retval = builder.CreateAlloca(retType, nullptr, ".retval");
        }

        endBB = llvm::BasicBlock::Create(context, "end");

        node->getBody()->gen(*this);

        fn->getBasicBlockList().push_back(endBB);
        builder.SetInsertPoint(endBB);
        if (node->getReturnType() != Type::VOID) {
            builder.CreateRet(builder.CreateLoad(retType, retval));
        } else {
            builder.CreateRetVoid();
        }

        return fn;
    }

    llvm::Value *Codegen::gen(FnCallNode *node) {
        auto name = node->getName();
        llvm::Function *callee = module.getFunction(name);
        if (!callee) {
            throw CodegenException("called function is not found: " + name);
        }
        if (callee->arg_size() != node->getArgs().size()) {
            throw CodegenException("callee args mismatch");
        }

        std::vector<llvm::Value *> args;
        for (auto &arg: node->getArgs()) {
            args.push_back(arg->gen(*this));
        }

        return builder.CreateCall(callee, args);
    }

    llvm::Value *Codegen::gen(ReturnNode *node) {
        if (!node->getVal()) {
            builder.CreateBr(endBB);
            return nullptr;
        }

        auto value = node->getVal()->gen(*this);
        if (!value) {
            throw CodegenException("return value is empty");
        }

        builder.CreateStore(value, retval);
        builder.CreateBr(endBB);

        return nullptr;
    }

    llvm::Value *Codegen::gen(BreakNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(ContinueNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(CommentNode *node) {
        return nullptr;
    }

    llvm::Type *Codegen::mapType(Type type) {
        switch (type) {
            case Type::INT:
                return llvm::Type::getInt64Ty(context);
            case Type::FLOAT:
                return llvm::Type::getFloatTy(context);
            case Type::BOOL:
                return llvm::Type::getInt1Ty(context);
            case Type::VOID:
                return llvm::Type::getVoidTy(context);
            default:
                throw CodegenException("invalid type");
        }
    }

    llvm::AllocaInst *Codegen::getVar(std::string &name) {
        auto var = namedValues[name];
        if (!var) {
            throw CodegenException("var not found: " + name);
        }
        return var;
    }
}
