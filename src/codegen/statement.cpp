#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(DeclareNode *node) {
        if (node->getType().getTypeID() == Type::TypeID::VOID) {
            throw InvalidTypeException();
        }

        auto type = mapType(node->getType());
        auto &name = node->getName();
        auto stackVar = createAlloca(type, name);

        auto value = node->getExpr() ?
                     node->getExpr()->gen(*this) :
                     createDefaultValue(node->getType());

        builder.CreateStore(value, stackVar);

        namedValues[name] = stackVar;
        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        auto value = node->getExpr()->gen(*this);
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

        loops.emplace(loopStartBB, loopEndBB);

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
        if (!node->getBody()->isLastNodeTerminate()) {
            builder.CreateBr(loopStartBB);
        }

        parentFunction->getBasicBlockList().push_back(loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();

        return nullptr;
    }

    llvm::Value *Codegen::gen(ForNode *node) {
        llvm::AllocaInst *idxVar = nullptr;
        auto &valVarName = node->getVal();
        auto arr = node->getExpr()->gen(*this);
        if (!arr) {
            throw InvalidTypeException();
        }
        auto arrType = deref(arr->getType());
        if (!Runtime::Array::isArrayType(arrType)) {
            // todo expected type
            throw InvalidTypeException();
        }

        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto loopInitBB = llvm::BasicBlock::Create(context, "loopInit", parentFunction);
        auto loopCondBB = llvm::BasicBlock::Create(context, "loopCond");
        auto loopBB = llvm::BasicBlock::Create(context, "loop");
        auto loopPostBB = llvm::BasicBlock::Create(context, "loopPost");
        auto loopEndBB = llvm::BasicBlock::Create(context, "loopEnd");

        loops.emplace(loopPostBB, loopEndBB);

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
            if (namedValues.contains(idxVarName)) {
                throw VarAlreadyExistsException(idxVarName);
            }
            idxVar = createAlloca(iterType, idxVarName);
            namedValues[idxVarName] = idxVar;
        }

        // init val
        if (namedValues.contains(valVarName)) {
            throw VarAlreadyExistsException(valVarName);
        }
        auto valVarType = arrayRuntime.getContainedType(llvm::cast<llvm::StructType>(arrType));
        if (!valVarType) {
            throw InvalidTypeException();
        }
        auto valVar = createAlloca(valVarType, valVarName);
        namedValues[valVarName] = valVar;

        // init stop value
        auto arrayLenFn = module.getFunction(mangler.mangleMethod(arrType->getStructName().str(), "length"));
        auto arrayLen = builder.CreateCall(arrayLenFn, {arr});

        builder.CreateBr(loopCondBB);

        // cond
        parentFunction->getBasicBlockList().push_back(loopCondBB);
        builder.SetInsertPoint(loopCondBB);

        llvm::Value *iter = builder.CreateLoad(iterType, iterVar);
        auto cond = builder.CreateICmpSLT(iter, arrayLen);
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        // body
        parentFunction->getBasicBlockList().push_back(loopBB);
        builder.SetInsertPoint(loopBB);

        // set idx
        if (node->hasIdx()) {
            builder.CreateStore(iter, idxVar);
        }

        // set val
        auto arrayGetFn = module.getFunction(mangler.mangleMethod(arrType->getStructName().str(), "get[]"));
        auto val = builder.CreateCall(arrayGetFn, {arr, iter});
        builder.CreateStore(val, valVar);

        // gen body
        node->getBody()->gen(*this);
        if (!node->getBody()->isLastNodeTerminate()) {
            builder.CreateBr(loopPostBB);
        }

        // post
        parentFunction->getBasicBlockList().push_back(loopPostBB);
        builder.SetInsertPoint(loopPostBB);

        iter = builder.CreateAdd(iter, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
        builder.CreateStore(iter, iterVar);
        builder.CreateBr(loopCondBB);

        // end
        parentFunction->getBasicBlockList().push_back(loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        namedValues.erase(valVarName);
        loops.pop();

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

        auto callee = module.getFunction("println");
        builder.CreateCall(callee, {value});

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

        auto arrSetFn = module.getFunction(mangler.mangleMethod(arrType->getStructName().str(), "set[]"));
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

        auto arrAppendFn = module.getFunction(mangler.mangleMethod(arrType->getStructName().str(), "append[]"));
        if (!arrAppendFn) {
            throw InvalidArrayAccessException();
        }
        builder.CreateCall(arrAppendFn, {arr, expr});

        return nullptr;
    }
}
