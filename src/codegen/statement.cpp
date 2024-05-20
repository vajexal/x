#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(DeclNode *node) {
        auto &name = node->getName();
        auto &vars = varScopes.back();
        if (vars.contains(name)) {
            throw VarAlreadyExistsException(name);
        }

        auto stackVar = createAlloca(mapType(node->getType()), name);

        auto value = node->getExpr() ?
                     castTo(node->getExpr()->gen(*this), node->getExpr()->getType(), node->getType()) :
                     createDefaultValue(node->getType());

        builder.CreateStore(value, stackVar);

        vars[name] = {stackVar, node->getType()};

        gcAddRoot(stackVar, node->getType());

        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        auto value = node->getExpr()->gen(*this);

        value = castTo(value, node->getExpr()->getType(), type);
        builder.CreateStore(value, var);

        return nullptr;
    }

    llvm::Value *Codegen::gen(IfNode *node) {
        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("if cond is empty");
        }
        cond = downcastToBool(cond, node->getCond()->getType());

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

        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("while cond is empty");
        }
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        parentFunction->insert(parentFunction->end(), loopBB);
        builder.SetInsertPoint(loopBB);
        node->getBody()->gen(*this);
        if (!node->getBody()->isLastNodeTerminate()) {
            builder.CreateBr(loopStartBB);
        }

        parentFunction->insert(parentFunction->end(), loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();

        return nullptr;
    }

    llvm::Value *Codegen::gen(ForNode *node) {
        auto &valVarName = node->getVal();
        auto range = llvm::dyn_cast<RangeNode>(node->getExpr());
        auto expr = node->getExpr()->gen(*this);
        auto exprType = node->getExpr()->getType();
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
        auto &iterName = node->hasIdx() ? node->getIdx() : mangler->mangleInternalSymbol("i");
        auto iterVar = createAlloca(iterType, iterName);
        builder.CreateStore(builder.getInt64(0), iterVar);

        // init val
        auto valVarType = range ? Type::scalar(Type::TypeID::INT) : *exprType.getSubtype();
        auto valVar = createAlloca(mapType(valVarType), valVarName);
        vars[valVarName] = {valVar, valVarType};

        // init idx
        if (node->hasIdx()) {
            vars[node->getIdx()] = {iterVar, Type::scalar(Type::TypeID::INT)};
        }

        // init start, stop, step
        llvm::Value *start, *stop, *step;
        if (range) {
            start = range->getStart() ? range->getStart()->gen(*this) : builder.getInt64(0);
            step = range->getStep() ? range->getStep()->gen(*this) : builder.getInt64(1);
            builder.CreateStore(start, valVar);

            auto dist = builder.CreateSub(range->getStop()->gen(*this), start);

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
        node->getBody()->gen(*this);
        if (!node->getBody()->isLastNodeTerminate()) {
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
        if (!node->getVal()) {
            builder.CreateRetVoid();
            return nullptr;
        }

        auto value = node->getVal()->gen(*this);
        if (!value) {
            throw CodegenException("return value is empty");
        }

        value = castTo(value, node->getVal()->getType(), currentFnRetType);

        builder.CreateRet(value);
        return nullptr;
    }

    llvm::Value *Codegen::gen(PrintlnNode *node) {
        auto value = node->getVal()->gen(*this);
        if (!value) {
            throw CodegenException("println value is empty");
        }

        auto &type = node->getVal()->getType();
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
        auto arr = node->getArr()->gen(*this);

        auto idx = node->getIdx()->gen(*this);
        auto expr = node->getExpr()->gen(*this);
        expr = castTo(expr, node->getExpr()->getType(), *node->getArr()->getType().getSubtype());

        auto arrSetFn = module.getFunction(mangler->mangleInternalMethod(Runtime::ArrayRuntime::getClassName(node->getArr()->getType()), "set[]"));
        if (!arrSetFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrSetFn, {arr, idx, expr});

        return nullptr;
    }

    llvm::Value *Codegen::gen(AppendArrNode *node) {
        auto arr = node->getArr()->gen(*this);

        auto expr = node->getExpr()->gen(*this);
        expr = castTo(expr, node->getExpr()->getType(), *node->getArr()->getType().getSubtype());

        auto arrAppendFn = module.getFunction(mangler->mangleInternalMethod(Runtime::ArrayRuntime::getClassName(node->getArr()->getType()), "append[]"));
        if (!arrAppendFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrAppendFn, {arr, expr});

        return nullptr;
    }
}
