#include "codegen.h"

#include "llvm/IR/Intrinsics.h"

#include "runtime/runtime.h"
#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(ScalarNode *node) {
        auto &type = node->getType();
        auto value = node->getValue();

        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return builder.getInt64(std::get<int64_t>(value));
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(builder.getDoubleTy(), std::get<double>(value));
            case Type::TypeID::BOOL:
                return builder.getInt1(std::get<bool>(value));
            case Type::TypeID::STRING: {
                auto str = newObj(llvm::StructType::getTypeByName(context, Runtime::String::CLASS_NAME));
                auto dataPtr = builder.CreateGlobalStringPtr(std::get<std::string>(value));
                builder.CreateCall(getInternalConstructor(Runtime::String::CLASS_NAME), {str, dataPtr});
                return str;
            }
            case Type::TypeID::ARRAY: {
                auto &exprList = std::get<ExprList>(value);
                std::vector<llvm::Value *> arrayValues;
                arrayValues.reserve(exprList.size());
                for (auto expr: exprList) {
                    arrayValues.push_back(expr->gen(*this));
                }
                auto arrType = getArrayForType(type);
                auto arr = newObj(arrType);
                auto len = builder.getInt64(exprList.size());
                builder.CreateCall(getInternalConstructor(arrType->getName().str()), {arr, len});
                fillArray(arr, type, arrayValues);
                return arr;
            }
            default:
                throw CodegenException("invalid scalar type");
        }
    }

    llvm::Value *Codegen::gen(UnaryNode *node) {
        auto opType = node->getOpType();
        auto expr = node->getExpr()->gen(*this);

        switch (opType) {
            case OpType::PRE_INC:
            case OpType::PRE_DEC:
            case OpType::POST_INC:
            case OpType::POST_DEC: {
                llvm::Value *value;
                switch (node->getExpr()->getType().getTypeID()) {
                    case Type::TypeID::INT:
                        value = opType == OpType::PRE_INC || opType == OpType::POST_INC ?
                                builder.CreateAdd(expr, builder.getInt64(1)) :
                                builder.CreateSub(expr, builder.getInt64(1));
                        break;
                    case Type::TypeID::FLOAT:
                        value = opType == OpType::PRE_INC || opType == OpType::POST_INC ?
                                builder.CreateFAdd(expr, llvm::ConstantFP::get(builder.getDoubleTy(), 1)) :
                                builder.CreateFSub(expr, llvm::ConstantFP::get(builder.getDoubleTy(), 1));
                        break;
                    default:
                        throw InvalidTypeException();
                }

                auto name = llvm::dyn_cast<VarNode>(node->getExpr())->getName();
                auto [type, var] = getVar(name);
                builder.CreateStore(value, var);

                return opType == OpType::PRE_INC || opType == OpType::PRE_DEC ? value : expr;
            }
            case OpType::NOT:
                expr = downcastToBool(expr, node->getExpr()->getType());
                return negate(expr);
            default:
                throw InvalidOpTypeException();
        }
    }

    llvm::Value *Codegen::gen(BinaryNode *node) {
        // logical "and" and "or" are special because of lazy evaluation
        if (node->getOpType() == OpType::AND) {
            return genLogicalAnd(node);
        } else if (node->getOpType() == OpType::OR) {
            return genLogicalOr(node);
        }

        auto lhs = node->getLhs()->gen(*this);
        auto lhsType = node->getLhs()->getType();
        auto rhs = node->getRhs()->gen(*this);
        auto rhsType = node->getRhs()->getType();

        if (!lhs || !rhs) {
            throw BinaryArgIsEmptyException();
        }

        if (lhsType.is(Type::TypeID::STRING) && rhsType.is(Type::TypeID::STRING)) {
            switch (node->getOpType()) {
                case OpType::PLUS: {
                    const auto &stringConcatFnName = Mangler::mangleInternalMethod(Runtime::String::CLASS_NAME, "concat");
                    auto stringConcatFn = module.getFunction(stringConcatFnName);
                    return builder.CreateCall(stringConcatFn, {lhs, rhs});
                }
                case OpType::EQUAL:
                    return compareStrings(lhs, rhs);
                case OpType::NOT_EQUAL:
                    return negate(compareStrings(lhs, rhs));
                default:
                    throw InvalidOpTypeException();
            }
        }

        if (node->getOpType() == OpType::DIV || node->getOpType() == OpType::POW) {
            std::tie(lhs, lhsType, rhs, rhsType) = forceUpcast(lhs, std::move(lhsType), rhs, std::move(rhsType));
        } else {
            std::tie(lhs, lhsType, rhs, rhsType) = upcast(lhs, std::move(lhsType), rhs, std::move(rhsType));
        }

        switch (lhsType.getTypeID()) {
            case Type::TypeID::INT: {
                switch (node->getOpType()) {
                    case OpType::PLUS:
                        return builder.CreateAdd(lhs, rhs);
                    case OpType::MINUS:
                        return builder.CreateSub(lhs, rhs);
                    case OpType::MUL:
                        return builder.CreateMul(lhs, rhs);
                    case OpType::MOD:
                        return builder.CreateSRem(lhs, rhs);
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
                        throw InvalidOpTypeException();
                }
            }
            case Type::TypeID::FLOAT: {
                switch (node->getOpType()) {
                    case OpType::PLUS:
                        return builder.CreateFAdd(lhs, rhs);
                    case OpType::MINUS:
                        return builder.CreateFSub(lhs, rhs);
                    case OpType::MUL:
                        return builder.CreateFMul(lhs, rhs);
                    case OpType::DIV:
                        return builder.CreateFDiv(lhs, rhs);
                    case OpType::POW:
                        return builder.CreateBinaryIntrinsic(llvm::Intrinsic::pow, lhs, rhs);
                    case OpType::MOD:
                        return builder.CreateFRem(lhs, rhs);
                    case OpType::EQUAL:
                        return builder.CreateFCmpOEQ(lhs, rhs);
                    case OpType::NOT_EQUAL:
                        return builder.CreateFCmpONE(lhs, rhs);
                    case OpType::SMALLER:
                        return builder.CreateFCmpOLT(lhs, rhs);
                    case OpType::SMALLER_OR_EQUAL:
                        return builder.CreateFCmpOLE(lhs, rhs);
                    case OpType::GREATER:
                        return builder.CreateFCmpOGT(lhs, rhs);
                    case OpType::GREATER_OR_EQUAL:
                        return builder.CreateFCmpOGE(lhs, rhs);
                    default:
                        throw InvalidOpTypeException();
                }
            }
            default:
                throw InvalidTypeException();
        }
    }

    llvm::Value *Codegen::gen(VarNode *node) {
        auto name = node->getName();
        if (name == THIS_KEYWORD && that) {
            return that->value;
        }

        auto [type, var] = getVar(name);
        return builder.CreateLoad(mapType(type), var, name);
    }

    llvm::Value *Codegen::gen(FetchArrNode *node) {
        auto arr = node->getArr()->gen(*this);
        auto idx = node->getIdx()->gen(*this);

        auto arrGetFn = module.getFunction(Mangler::mangleInternalMethod(Runtime::ArrayRuntime::getClassName(node->getArr()->getType()), "get[]"));
        if (!arrGetFn) {
            throw InvalidArrayAccessException();
        }

        return builder.CreateCall(arrGetFn, {arr, idx});
    }

    llvm::Value *Codegen::genLogicalAnd(BinaryNode *node) {
        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto thenBB = llvm::BasicBlock::Create(context);
        auto mergeBB = llvm::BasicBlock::Create(context);

        auto lhs = node->getLhs()->gen(*this);
        if (!lhs) {
            throw BinaryArgIsEmptyException();
        }
        lhs = downcastToBool(lhs, node->getLhs()->getType());
        builder.CreateCondBr(lhs, thenBB, mergeBB);
        auto lhsBB = builder.GetInsertBlock();

        parentFunction->insert(parentFunction->end(), thenBB);
        builder.SetInsertPoint(thenBB);
        auto rhs = node->getRhs()->gen(*this);
        if (!rhs) {
            throw BinaryArgIsEmptyException();
        }
        rhs = downcastToBool(rhs, node->getRhs()->getType());
        builder.CreateBr(mergeBB);
        auto rhsBB = builder.GetInsertBlock();

        parentFunction->insert(parentFunction->end(), mergeBB);
        builder.SetInsertPoint(mergeBB);
        auto phi = builder.CreatePHI(builder.getInt1Ty(), 2);
        phi->addIncoming(builder.getFalse(), lhsBB);
        phi->addIncoming(rhs, rhsBB);
        return phi;
    }

    llvm::Value *Codegen::genLogicalOr(BinaryNode *node) {
        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto elseBB = llvm::BasicBlock::Create(context);
        auto mergeBB = llvm::BasicBlock::Create(context);

        auto lhs = node->getLhs()->gen(*this);
        if (!lhs) {
            throw BinaryArgIsEmptyException();
        }
        lhs = downcastToBool(lhs, node->getLhs()->getType());
        builder.CreateCondBr(lhs, mergeBB, elseBB);
        auto lhsBB = builder.GetInsertBlock();

        parentFunction->insert(parentFunction->end(), elseBB);
        builder.SetInsertPoint(elseBB);
        auto rhs = node->getRhs()->gen(*this);
        if (!rhs) {
            throw BinaryArgIsEmptyException();
        }
        rhs = downcastToBool(rhs, node->getRhs()->getType());
        builder.CreateBr(mergeBB);
        auto rhsBB = builder.GetInsertBlock();

        parentFunction->insert(parentFunction->end(), mergeBB);
        builder.SetInsertPoint(mergeBB);
        auto phi = builder.CreatePHI(builder.getInt1Ty(), 2);
        phi->addIncoming(builder.getTrue(), lhsBB);
        phi->addIncoming(rhs, rhsBB);
        return phi;
    }
}
