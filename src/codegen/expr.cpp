#include "codegen.h"

#include "runtime/runtime.h"
#include "utils.h"

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
                std::vector<llvm::Value *> arrayValues;
                arrayValues.reserve(exprList.size());
                for (auto expr: exprList) {
                    arrayValues.push_back(expr->gen(*this));
                }
                // todo check all elem types are the same
                auto arrType = getArrayForType(type.getSubtype());
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

        if (Runtime::String::isStringType(lhs->getType()) && Runtime::String::isStringType(rhs->getType())) {
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

    llvm::Value *Codegen::gen(VarNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        return builder.CreateLoad(type, var, name);
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
}
