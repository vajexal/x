#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(DeclNode *node) {
        auto &vars = varScopes.back();
        if (vars.contains(node->name)) {
            throw VarAlreadyExistsException(node->name);
        }

        auto stackVar = createAlloca(mapType(node->type), node->name);

        auto value = node->expr ?
                     castTo(node->expr->gen(*this), node->expr->type, node->type) :
                     createDefaultValue(node->type);

        builder.CreateStore(value, stackVar);

        vars[node->name] = {stackVar, node->type};

        gcAddRoot(stackVar, node->type);

        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignNode *node) {
        auto name = node->name;
        auto [type, var] = getVar(name);
        auto value = node->expr->gen(*this);

        value = castTo(value, node->expr->type, type);
        builder.CreateStore(value, var);

        return nullptr;
    }

    llvm::Value *Codegen::gen(IfNode *node) {
        auto cond = node->cond->gen(*this);
        if (!cond) {
            throw CodegenException("if cond is empty");
        }
        cond = downcastToBool(cond, node->cond->type);

        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto thenBB = llvm::BasicBlock::Create(context, "then", parentFunction);
        auto elseBB = llvm::BasicBlock::Create(context, "else");
        auto mergeBB = llvm::BasicBlock::Create(context, "ifcont");

        auto thenNode = node->thenNode;
        auto elseNode = node->elseNode;

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
            parentFunction->insert(parentFunction->end(), elseBB);
            builder.SetInsertPoint(elseBB);
            elseNode->gen(*this);
            if (!elseNode->isLastNodeTerminate()) {
                builder.CreateBr(mergeBB);
            }
        }

        parentFunction->insert(parentFunction->end(), mergeBB);
        builder.SetInsertPoint(mergeBB);

        return nullptr;
    }

    llvm::Value *Codegen::gen(WhileNode *node) {
        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto loopStartBB = llvm::BasicBlock::Create(context, "loopStart");
        auto loopBB = llvm::BasicBlock::Create(context, "loop");
        auto loopEndBB = llvm::BasicBlock::Create(context, "loopEnd");

        loops.emplace(loopStartBB, loopEndBB);

        builder.CreateBr(loopStartBB);
        parentFunction->insert(parentFunction->end(), loopStartBB);
        builder.SetInsertPoint(loopStartBB);

        auto cond = node->cond->gen(*this);
        if (!cond) {
            throw CodegenException("while cond is empty");
        }
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        parentFunction->insert(parentFunction->end(), loopBB);
        builder.SetInsertPoint(loopBB);
        node->body->gen(*this);
        if (!node->body->isLastNodeTerminate()) {
            builder.CreateBr(loopStartBB);
        }

        parentFunction->insert(parentFunction->end(), loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();

        return nullptr;
    }

    llvm::Value *Codegen::gen(ForNode *node) {
        auto &valVarName = node->val;
        auto range = llvm::dyn_cast<RangeNode>(node->expr);
        auto expr = node->expr->gen(*this);
        auto exprType = node->expr->type;
        auto arrTypeName = Runtime::ArrayRuntime::getClassName(exprType);

        if (!range) {
            // add expr gc root
            auto exprAlloc = createAlloca(expr->getType());
            builder.CreateStore(expr, exprAlloc);
            gcAddRoot(exprAlloc, exprType);
        }

        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto loopInitBB = llvm::BasicBlock::Create(context, "loopInit", parentFunction);
        auto loopCondBB = llvm::BasicBlock::Create(context, "loopCond");
        auto loopBB = llvm::BasicBlock::Create(context, "loop");
        auto loopPostBB = llvm::BasicBlock::Create(context, "loopPost");
        auto loopEndBB = llvm::BasicBlock::Create(context, "loopEnd");

        loops.emplace(loopPostBB, loopEndBB);
        varScopes.emplace_back();
        auto &vars = varScopes.back();

        builder.CreateBr(loopInitBB);

        // init
        builder.SetInsertPoint(loopInitBB);

        // init iter
        auto iterType = builder.getInt64Ty();
        auto iterName = node->idx ? node->idx.value() : mangler->mangleInternalSymbol("i");
        auto iterVar = createAlloca(iterType, iterName);
        builder.CreateStore(builder.getInt64(0), iterVar);

        // init val
        auto valVarType = range ? Type::scalar(Type::TypeID::INT) : *exprType.getSubtype();
        auto valVar = createAlloca(mapType(valVarType), valVarName);
        vars[valVarName] = {valVar, valVarType};

        // init idx
        if (node->idx) {
            vars[node->idx.value()] = {iterVar, Type::scalar(Type::TypeID::INT)};
        }

        // init start, stop, step
        llvm::Value *start, *stop, *step;
        if (range) {
            start = range->start ? range->start->gen(*this) : builder.getInt64(0);
            step = range->step ? range->step->gen(*this) : builder.getInt64(1);
            builder.CreateStore(start, valVar);

            auto dist = builder.CreateSub(range->stop->gen(*this), start);

            // ceil(dist / step)
            stop = builder.CreateFDiv(builder.CreateSIToFP(dist, builder.getDoubleTy()), builder.CreateSIToFP(step, builder.getDoubleTy()));
            stop = builder.CreateUnaryIntrinsic(llvm::Intrinsic::ceil, stop);
            stop = builder.CreateFPToSI(stop, builder.getInt64Ty());

            // check finiteness
            auto loopInitContBB = llvm::BasicBlock::Create(context, "loopInitCont");
            auto finiteness = builder.CreateICmpSGE(builder.CreateXor(dist, step), builder.getInt64(0));
            builder.CreateCondBr(finiteness, loopInitContBB, loopEndBB);

            parentFunction->insert(parentFunction->end(), loopInitContBB);
            builder.SetInsertPoint(loopInitContBB);

            // check step
            auto invalidStepBB = llvm::BasicBlock::Create(context, "invalid_step");
            auto cond = builder.CreateICmpEQ(step, builder.getInt64(0));
            builder.CreateCondBr(cond, invalidStepBB, loopCondBB);

            parentFunction->insert(parentFunction->end(), invalidStepBB);
            builder.SetInsertPoint(invalidStepBB);

            // print error and exit
            auto dieFn = module.getFunction(mangler->mangleInternalFunction("die"));
            auto message = builder.CreateGlobalStringPtr("step must not be zero");
            builder.CreateCall(dieFn, {message});
            builder.CreateUnreachable();
        } else {
            start = builder.getInt64(0);
            stop = builder.CreateCall(module.getFunction(mangler->mangleInternalMethod(arrTypeName, "length")), {expr});
            step = builder.getInt64(1);

            builder.CreateBr(loopCondBB);
        }

        // cond
        parentFunction->insert(parentFunction->end(), loopCondBB);
        builder.SetInsertPoint(loopCondBB);

        llvm::Value *iter = builder.CreateLoad(iterType, iterVar);
        auto cond = builder.CreateICmpSLT(iter, stop);
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        // body
        parentFunction->insert(parentFunction->end(), loopBB);
        builder.SetInsertPoint(loopBB);

        // set val
        if (!range) {
            auto val = builder.CreateCall(module.getFunction(mangler->mangleInternalMethod(arrTypeName, "get[]")), {expr, iter});
            builder.CreateStore(val, valVar);
        }

        // gen body
        node->body->gen(*this);
        if (!node->body->isLastNodeTerminate()) {
            builder.CreateBr(loopPostBB);
        }

        // post
        parentFunction->insert(parentFunction->end(), loopPostBB);
        builder.SetInsertPoint(loopPostBB);

        iter = builder.CreateAdd(iter, builder.getInt64(1));
        builder.CreateStore(iter, iterVar);

        if (range) {
            // calc val
            auto val = builder.CreateLoad(builder.getInt64Ty(), valVar);
            auto newVal = builder.CreateAdd(val, step);
            builder.CreateStore(newVal, valVar);
        }

        builder.CreateBr(loopCondBB);

        // end
        parentFunction->insert(parentFunction->end(), loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();
        varScopes.pop_back();

        return nullptr;
    }

    llvm::Value *Codegen::gen(RangeNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(BreakNode *node) {
        if (loops.empty()) {
            throw CodegenException("using break outside of loop");
        }

        auto loop = loops.top();
        builder.CreateBr(loop.end);

        return nullptr;
    }

    llvm::Value *Codegen::gen(ContinueNode *node) {
        if (loops.empty()) {
            throw CodegenException("using continue outside of loop");
        }

        auto loop = loops.top();
        builder.CreateBr(loop.start);

        return nullptr;
    }

    llvm::Value *Codegen::gen(ReturnNode *node) {
        if (!node->val) {
            builder.CreateRetVoid();
            return nullptr;
        }

        auto value = node->val->gen(*this);
        if (!value) {
            throw CodegenException("return value is empty");
        }

        value = castTo(value, node->val->type, currentFnRetType);

        builder.CreateRet(value);
        return nullptr;
    }

    llvm::Value *Codegen::gen(PrintlnNode *node) {
        auto value = node->val->gen(*this);
        if (!value) {
            throw CodegenException("println value is empty");
        }

        auto &type = node->val->type;
        auto printFn = module.getFunction(mangler->mangleInternalFunction("print"));

        if (type.is(Type::TypeID::ARRAY)) {
            builder.CreateCall(printFn, {
                    builder.getInt64(static_cast<uint64_t>(type.getTypeID())),
                    builder.getInt64(static_cast<uint64_t>(type.getSubtype()->getTypeID())),
                    value
            });
        } else {
            builder.CreateCall(printFn, {
                    builder.getInt64(static_cast<uint64_t>(type.getTypeID())),
                    value
            });
        }

        builder.CreateCall(module.getFunction(mangler->mangleInternalFunction("printNewline")));

        return nullptr;
    }

    llvm::Value *Codegen::gen(CommentNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignArrNode *node) {
        auto arr = node->arr->gen(*this);

        auto idx = node->idx->gen(*this);
        auto expr = node->expr->gen(*this);
        expr = castTo(expr, node->expr->type, *node->arr->type.getSubtype());

        auto arrSetFn = module.getFunction(mangler->mangleInternalMethod(Runtime::ArrayRuntime::getClassName(node->arr->type), "set[]"));
        if (!arrSetFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrSetFn, {arr, idx, expr});

        return nullptr;
    }

    llvm::Value *Codegen::gen(AppendArrNode *node) {
        auto arr = node->arr->gen(*this);

        auto expr = node->expr->gen(*this);
        expr = castTo(expr, node->expr->type, *node->arr->type.getSubtype());

        auto arrAppendFn = module.getFunction(mangler->mangleInternalMethod(Runtime::ArrayRuntime::getClassName(node->arr->type), "append[]"));
        if (!arrAppendFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrAppendFn, {arr, expr});

        return nullptr;
    }
}
