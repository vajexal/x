#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(DeclNode *node) {
        auto &name = node->getName();
        auto &vars = varScopes.back();
        if (vars.contains(name)) {
            throw VarAlreadyExistsException(name);
        }

        auto type = mapType(node->getType());
        auto stackVar = createAlloca(type, name);

        auto value = node->getExpr() ?
                     node->getExpr()->gen(*this) :
                     createDefaultValue(node->getType());

        value = castTo(value, type);

        builder.CreateStore(value, stackVar);

        vars[name] = stackVar;

        gcAddRoot(stackVar);

        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        auto value = node->getExpr()->gen(*this);

        value = castTo(value, type);
        builder.CreateStore(value, var);

        return nullptr;
    }

    llvm::Value *Codegen::gen(IfNode *node) {
        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("if cond is empty");
        }
        cond = downcastToBool(cond);

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
        auto isRange = llvm::isa<RangeNode>(node->getExpr());
        auto expr = node->getExpr()->gen(*this);
        auto exprType = deref(expr->getType());

        // add expr gc root
        auto exprAlloc = createAlloca(expr->getType());
        builder.CreateStore(expr, exprAlloc);
        gcAddRoot(exprAlloc);

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
        auto iterType = llvm::Type::getInt64Ty(context);
        auto iterVar = createAlloca(iterType, "i");
        auto iterStartValue = llvm::ConstantInt::getSigned(iterType, 0);
        builder.CreateStore(iterStartValue, iterVar);

        // init idx
        if (node->hasIdx()) {
            auto &idxVarName = node->getIdx();
            idxVar = createAlloca(iterType, idxVarName);
            vars[idxVarName] = idxVar;
        }

        // init val
        auto valVarType = isRange ?
                          llvm::Type::getInt64Ty(context) :
                          arrayRuntime.getContainedType(llvm::cast<llvm::StructType>(exprType));
        if (!valVarType) {
            throw InvalidTypeException();
        }
        auto valVar = createAlloca(valVarType, valVarName);
        vars[valVarName] = valVar;

        // init stop value
        auto lengthFn = module.getFunction(mangler.mangleInternalMethod(exprType->getStructName().str(), "length"));
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
        auto getFn = module.getFunction(mangler.mangleInternalMethod(exprType->getStructName().str(), "get[]"));
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

        iter = builder.CreateAdd(iter, llvm::ConstantInt::getSigned(iterType, 1));
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
                     llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0);
        auto stop = node->getStop()->gen(*this);
        auto step = node->getStep() ?
                    node->getStep()->gen(*this) :
                    llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1);

        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto invalidStepBB = llvm::BasicBlock::Create(context, "invalid_step", parentFunction);
        auto mergeBB = llvm::BasicBlock::Create(context, "merge");

        // validate step
        auto cond = builder.CreateICmpEQ(step, builder.getInt64(0));
        builder.CreateCondBr(cond, invalidStepBB, mergeBB);

        builder.SetInsertPoint(invalidStepBB);

        // print error and exit
        auto dieFn = module.getFunction(mangler.mangleInternalFunction("die"));
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

        value = castTo(value, builder.getCurrentFunctionReturnType());

        builder.CreateRet(value);
        return nullptr;
    }

    llvm::Value *Codegen::gen(PrintlnNode *node) {
        auto value = node->getVal()->gen(*this);
        if (!value) {
            throw CodegenException("println value is empty");
        }

        if (!value->getType()->isStructTy()) {
            value = castToString(value);
        }

        auto printlnFn = module.getFunction("println");
        builder.CreateCall(printlnFn, {value});

        return nullptr;
    }

    llvm::Value *Codegen::gen(CommentNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignArrNode *node) {
        auto arr = node->getArr()->gen(*this);
        auto arrType = deref(arr->getType());
        if (!Runtime::Array::isArrayType(arrType)) {
            throw InvalidArrayAccessException();
        }

        auto idx = node->getIdx()->gen(*this);
        auto expr = node->getExpr()->gen(*this);
        auto arrElemType = arrayRuntime.getContainedType(llvm::cast<llvm::StructType>(arrType));
        expr = castTo(expr, arrElemType);

        auto arrSetFn = module.getFunction(mangler.mangleInternalMethod(arrType->getStructName().str(), "set[]"));
        if (!arrSetFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrSetFn, {arr, idx, expr});

        return nullptr;
    }

    llvm::Value *Codegen::gen(AppendArrNode *node) {
        auto arr = node->getArr()->gen(*this);
        auto arrType = deref(arr->getType());
        if (!Runtime::Array::isArrayType(arrType)) {
            throw InvalidArrayAccessException();
        }

        auto expr = node->getExpr()->gen(*this);
        auto arrElemType = arrayRuntime.getContainedType(llvm::cast<llvm::StructType>(arrType));
        expr = castTo(expr, arrElemType);

        auto arrAppendFn = module.getFunction(mangler.mangleInternalMethod(arrType->getStructName().str(), "append[]"));
        if (!arrAppendFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrAppendFn, {arr, expr});

        return nullptr;
    }
}
