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
        llvm::AllocaInst *idxVar = nullptr;
        auto &valVarName = node->getVal();
        auto expr = node->getExpr()->gen(*this);
        auto exprClassName = getClassName(node->getExpr()->getType());

        // add expr gc root
        auto exprAlloc = createAlloca(expr->getType());
        builder.CreateStore(expr, exprAlloc);
        gcAddRoot(exprAlloc, node->getExpr()->getType());

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
        auto iterVar = createAlloca(iterType, "i");
        auto iterStartValue = builder.getInt64(0);
        builder.CreateStore(iterStartValue, iterVar);

        // init idx
        if (node->hasIdx()) {
            auto &idxVarName = node->getIdx();
            idxVar = createAlloca(iterType, idxVarName);
            vars[idxVarName] = {idxVar, Type::scalar(Type::TypeID::INT)};
        }

        // init val
        auto valVarType = node->getExpr()->getType().is(Type::TypeID::ARRAY) ?
                          *node->getExpr()->getType().getSubtype() :
                          Type::scalar(Type::TypeID::INT);
        auto valVar = createAlloca(mapType(valVarType), valVarName);
        vars[valVarName] = {valVar, valVarType};

        // init stop value
        auto lengthFn = module.getFunction(Mangler::mangleInternalMethod(exprClassName, "length"));
        auto iterStopValue = builder.CreateCall(lengthFn, {expr});

        builder.CreateBr(loopCondBB);

        // cond
        parentFunction->insert(parentFunction->end(), loopCondBB);
        builder.SetInsertPoint(loopCondBB);

        llvm::Value *iter = builder.CreateLoad(iterType, iterVar);
        auto cond = builder.CreateICmpSLT(iter, iterStopValue);
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        // body
        parentFunction->insert(parentFunction->end(), loopBB);
        builder.SetInsertPoint(loopBB);

        // set idx
        if (node->hasIdx()) {
            builder.CreateStore(iter, idxVar);
        }

        // set val
        auto getFn = module.getFunction(Mangler::mangleInternalMethod(exprClassName, "get[]"));
        auto val = builder.CreateCall(getFn, {expr, iter});
        builder.CreateStore(val, valVar);

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
        builder.CreateBr(loopCondBB);

        // end
        parentFunction->insert(parentFunction->end(), loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();
        varScopes.pop_back();

        return nullptr;
    }

    llvm::Value *Codegen::gen(RangeNode *node) {
        auto start = node->getStart() ?
                     node->getStart()->gen(*this) :
                     builder.getInt64(0);
        auto stop = node->getStop()->gen(*this);
        auto step = node->getStep() ?
                    node->getStep()->gen(*this) :
                    builder.getInt64(1);

        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto invalidStepBB = llvm::BasicBlock::Create(context, "invalid_step", parentFunction);
        auto mergeBB = llvm::BasicBlock::Create(context, "merge");

        // validate step
        auto cond = builder.CreateICmpEQ(step, builder.getInt64(0));
        builder.CreateCondBr(cond, invalidStepBB, mergeBB);

        builder.SetInsertPoint(invalidStepBB);

        // print error and exit
        auto dieFn = module.getFunction(Mangler::mangleInternalFunction("die"));
        auto message = builder.CreateGlobalStringPtr("step must not be zero");
        builder.CreateCall(dieFn, {message});
        builder.CreateUnreachable();

        parentFunction->insert(parentFunction->end(), mergeBB);
        builder.SetInsertPoint(mergeBB);

        // create range object
        auto rangeType = llvm::StructType::getTypeByName(context, Runtime::Range::CLASS_NAME);
        auto rangeObj = newObj(rangeType);

        auto startPtr = builder.CreateStructGEP(rangeType, rangeObj, 0);
        builder.CreateStore(start, startPtr);
        auto stopPtr = builder.CreateStructGEP(rangeType, rangeObj, 1);
        builder.CreateStore(stop, stopPtr);
        auto stepPtr = builder.CreateStructGEP(rangeType, rangeObj, 2);
        builder.CreateStore(step, stepPtr);

        return rangeObj;
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

        value = castToString(value, node->getVal()->getType());

        auto printlnFn = module.getFunction("println");
        builder.CreateCall(printlnFn, {value});

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

        auto arrSetFn = module.getFunction(Mangler::mangleInternalMethod(Runtime::Array::getClassName(node->getArr()->getType()), "set[]"));
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

        auto arrAppendFn = module.getFunction(Mangler::mangleInternalMethod(Runtime::Array::getClassName(node->getArr()->getType()), "append[]"));
        if (!arrAppendFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrAppendFn, {arr, expr});

        return nullptr;
    }
}
