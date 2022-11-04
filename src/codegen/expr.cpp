#include "codegen.h"
#include "runtime/runtime.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(ScalarNode *node) {
        auto &type = node->getType();
        auto value = node->getValue();

        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), std::get<int>(value));
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getFloatTy(context), std::get<float>(value));
            case Type::TypeID::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), std::get<bool>(value));
            case Type::TypeID::STRING: {
                auto str = createAlloca(llvm::StructType::getTypeByName(context, Runtime::String::CLASS_NAME));
                auto dataPtr = builder.CreateGlobalStringPtr(std::get<std::string>(value));
                builder.CreateCall(getConstructor(Runtime::String::CLASS_NAME), {str, dataPtr});
                return str;
            }
            case Type::TypeID::ARRAY: {
                auto &exprList = std::get<ExprList>(value);
                if (exprList.empty()) {
                    // we won't be able to get first element type to determine array types
                    throw CodegenException("cannot create empty array literal");
                }
                std::vector<llvm::Value *> arrayValues;
                arrayValues.reserve(exprList.size());
                for (auto expr: exprList) {
                    arrayValues.push_back(expr->gen(*this));
                }
                // todo check all elem types are the same
                auto arrType = getArrayForType(arrayValues[0]->getType());
                auto arr = createAlloca(arrType);
                auto len = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), (int64_t)exprList.size());
                builder.CreateCall(getConstructor(arrType->getName().str()), {arr, len});
                fillArray(arr, arrayValues);
                return arr;
            }
            default:
                throw CodegenException("invalid scalar type");
        }
    }

    llvm::Value *Codegen::gen(UnaryNode *node) {
        auto opType = node->getType();
        auto expr = node->getExpr()->gen(*this);

        switch (opType) {
            case OpType::PRE_INC:
            case OpType::PRE_DEC:
            case OpType::POST_INC:
            case OpType::POST_DEC: {
                llvm::Value *value;
                switch (expr->getType()->getTypeID()) {
                    case llvm::Type::TypeID::IntegerTyID:
                        value = opType == OpType::PRE_INC || opType == OpType::POST_INC ?
                                builder.CreateAdd(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1)) :
                                builder.CreateSub(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
                        break;
                    case llvm::Type::TypeID::FloatTyID:
                        value = opType == OpType::PRE_INC || opType == OpType::POST_INC ?
                                builder.CreateFAdd(expr, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 1)) :
                                builder.CreateFSub(expr, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 1));
                        break;
                    default:
                        throw CodegenException("invalid type");
                }

                auto name = dynamic_cast<VarNode *>(node->getExpr())->getName(); // todo
                auto [type, var] = getVar(name);
                builder.CreateStore(value, var);

                return opType == OpType::PRE_INC || opType == OpType::PRE_DEC ? value : expr;
            }
            case OpType::NOT:
                expr = downcastToBool(expr);
                return negate(expr);
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

        if (isStringType(lhs->getType()) && isStringType(rhs->getType())) {
            switch (node->getType()) {
                case OpType::PLUS: {
                    auto stringConcatFnName = mangler.mangleMethod(Runtime::String::CLASS_NAME, "concat");
                    auto callee = module.getFunction(stringConcatFnName);
                    return builder.CreateCall(callee, {lhs, rhs});
                }
                case OpType::EQUAL:
                    return compareStrings(lhs, rhs);
                case OpType::NOT_EQUAL:
                    return negate(compareStrings(lhs, rhs));
                default:
                    throw CodegenException("invalid op type");
            }
        }

        if (node->getType() == OpType::DIV) {
            std::tie(lhs, rhs) = forceUpcast(lhs, rhs);
        } else {
            std::tie(lhs, rhs) = upcast(lhs, rhs);
        }

        using binaryExprGenerator = std::function<llvm::Value *(llvm::Value *, llvm::Value *)>;

        // todo maybe refactor to switches
        static std::map<llvm::Type::TypeID, std::map<OpType, binaryExprGenerator>> binaryExprGeneratorMap{
                {llvm::Type::TypeID::IntegerTyID, {
                        {OpType::PLUS, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateAdd(lhs, rhs); }},
                        {OpType::MINUS, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateSub(lhs, rhs); }},
                        {OpType::MUL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateMul(lhs, rhs); }},
                        {OpType::EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpEQ(lhs, rhs); }},
                        {OpType::NOT_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpNE(lhs, rhs); }},
                        {OpType::SMALLER, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSLT(lhs, rhs); }},
                        {OpType::SMALLER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSLE(lhs, rhs); }},
                        {OpType::GREATER, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSGT(lhs, rhs); }},
                        {OpType::GREATER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSGE(lhs, rhs); }}
                }},
                {llvm::Type::TypeID::FloatTyID, {
                        {OpType::PLUS, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFAdd(lhs, rhs); }},
                        {OpType::MINUS, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFSub(lhs, rhs); }},
                        {OpType::MUL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFMul(lhs, rhs); }},
                        {OpType::DIV, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFDiv(lhs, rhs); }},
                        {OpType::EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOEQ(lhs, rhs); }},
                        {OpType::NOT_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpONE(lhs, rhs); }},
                        {OpType::SMALLER, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOLT(lhs, rhs); }},
                        {OpType::SMALLER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOLE(lhs, rhs); }},
                        {OpType::GREATER, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOGT(lhs, rhs); }},
                        {OpType::GREATER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOGE(lhs, rhs); }}
                }}
        };

        auto typeGeneratorMap = binaryExprGeneratorMap.find(lhs->getType()->getTypeID());
        if (typeGeneratorMap == binaryExprGeneratorMap.end()) {
            throw CodegenException("invalid type");
        }

        auto exprGenerator = typeGeneratorMap->second.find(node->getType());
        if (exprGenerator == typeGeneratorMap->second.end()) {
            throw CodegenException("invalid op type");
        }

        return exprGenerator->second(lhs, rhs);
    }

    llvm::Value *Codegen::gen(DeclareNode *node) {
        if (node->getType().getTypeID() == Type::TypeID::VOID) {
            throw CodegenException("invalid type");
        }

        auto type = mapType(node->getType());
        auto &name = node->getName();
        auto stackVar = createAlloca(type, name);
        auto value = node->getExpr()->gen(*this);
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

    llvm::Value *Codegen::gen(VarNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        return builder.CreateLoad(type, var, name);
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
        builder.CreateBr(loopStartBB);

        parentFunction->getBasicBlockList().push_back(loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();

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

    llvm::Value *Codegen::gen(CommentNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(FetchArrNode *node) {
        auto arr = node->getArr()->gen(*this);
        auto idx = node->getIdx()->gen(*this);
        auto arrType = deref(arr->getType());
        auto arrGetFn = module.getFunction(mangler.mangleMethod(arrType->getStructName().str(), "get[]"));
        if (!arrGetFn) {
            throw CodegenException("invalid [] operation");
        }
        return builder.CreateCall(arrGetFn, {arr, idx});
    }

    llvm::Value *Codegen::gen(AssignArrNode *node) {
        auto arr = node->getArr()->gen(*this);
        auto idx = node->getIdx()->gen(*this);
        auto expr = node->getExpr()->gen(*this);
        auto arrType = deref(arr->getType());
        auto arrSetFn = module.getFunction(mangler.mangleMethod(arrType->getStructName().str(), "set[]"));
        if (!arrSetFn) {
            throw CodegenException("invalid [] operation");
        }
        builder.CreateCall(arrSetFn, {arr, idx, expr});
        return nullptr;
    }
}
