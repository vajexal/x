#include "codegen.h"

#include <fmt/core.h>

#include "runtime/runtime.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(Node *node) {
        throw CodegenException("can't gen node");
    }

    llvm::Value *Codegen::gen(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            child->gen(*this);
        }

        return nullptr;
    }

    llvm::Type *Codegen::mapType(const Type &type) const {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::FLOAT:
                return llvm::Type::getFloatTy(context);
            case Type::TypeID::BOOL:
                return llvm::Type::getInt1Ty(context);
            case Type::TypeID::STRING:
                return llvm::StructType::getTypeByName(context, Runtime::String::CLASS_NAME)->getPointerTo();
            case Type::TypeID::VOID:
                return llvm::Type::getVoidTy(context);
            case Type::TypeID::CLASS: {
                auto &className = type.getClassName();
                auto classDecl = getClass(mangler.mangleClass(className));
                return classDecl.type->getPointerTo();
            }
            default:
                throw CodegenException("invalid type");
        }
    }

    llvm::Constant *Codegen::getDefaultValue(const Type &type) const {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0);
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 0);
            case Type::TypeID::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            default:
                throw CodegenException("invalid type");
        }
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getVar(std::string &name) const {
        auto var = namedValues.find(name);
        if (var != namedValues.end()) {
            return {var->second->getAllocatedType(), var->second};
        }

        if (that) {
            auto classType = deref(that->getType());
            auto classDecl = getClass(classType->getStructName().str());
            auto prop = classDecl.props.find(name);
            if (prop != classDecl.props.end()) {
                auto ptr = builder.CreateStructGEP(classDecl.type, that, prop->second.pos);
                return {prop->second.type, ptr};
            }

            auto currentClassDecl = classDecl.parent;
            while (currentClassDecl) {
                prop = currentClassDecl->props.find(name);
                if (prop != currentClassDecl->props.end()) {
                    if (prop->second.accessModifier == AccessModifier::PRIVATE) {
                        throw CodegenException("cannot access private property: " + name);
                    }
                    auto parent = builder.CreateBitCast(that, currentClassDecl->type->getPointerTo());
                    auto ptr = builder.CreateStructGEP(currentClassDecl->type, parent, prop->second.pos);
                    return {prop->second.type, ptr};
                }

                currentClassDecl = currentClassDecl->parent;
            }
        }

        if (self) {
            auto currentClassDecl = self;
            while (currentClassDecl) {
                auto prop = currentClassDecl->staticProps.find(name);
                if (prop != currentClassDecl->staticProps.end()) {
                    if (currentClassDecl != self && prop->second.accessModifier == AccessModifier::PRIVATE) {
                        throw CodegenException("cannot access private property: " + name);
                    }
                    return {prop->second.var->getType()->getPointerElementType(), prop->second.var};
                }

                currentClassDecl = currentClassDecl->parent;
            }
        }

        throw CodegenException("var not found: " + name);
    }

    llvm::AllocaInst *Codegen::createAlloca(llvm::Type *type, const std::string &name) const {
        auto fn = builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        return tmpBuilder.CreateAlloca(type, nullptr, name);
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::upcast(llvm::Value *a, llvm::Value *b) const {
        if (a->getType()->isFloatTy() && b->getType()->isIntegerTy()) {
            return {a, builder.CreateSIToFP(b, llvm::Type::getFloatTy(context))};
        }

        if (a->getType()->isIntegerTy() && b->getType()->isFloatTy()) {
            return {builder.CreateSIToFP(a, llvm::Type::getFloatTy(context)), b};
        }

        return {a, b};
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::forceUpcast(llvm::Value *a, llvm::Value *b) const {
        if (a->getType()->isIntegerTy()) {
            a = builder.CreateSIToFP(a, llvm::Type::getFloatTy(context));
        }

        if (b->getType()->isIntegerTy()) {
            b = builder.CreateSIToFP(b, llvm::Type::getFloatTy(context));
        }

        return {a, b};
    }

    llvm::Value *Codegen::downcastToBool(llvm::Value *value) const {
        if (value->getType()->isIntegerTy(1)) {
            return value;
        }

        switch (value->getType()->getTypeID()) {
            case llvm::Type::TypeID::IntegerTyID:
                return builder.CreateICmpNE(value, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0));
            case llvm::Type::TypeID::FloatTyID:
                return builder.CreateFCmpONE(value, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 0));
            default:
                if (isStringType(value->getType())) {
                    auto stringIsEmptyFnName = mangler.mangleMethod(Runtime::String::CLASS_NAME, "isEmpty");
                    auto callee = module.getFunction(stringIsEmptyFnName);
                    auto val = builder.CreateCall(callee, {value});
                    return negate(val);
                }
                throw CodegenException("invalid type");
        }
    }

    llvm::Value *Codegen::castToString(llvm::Value *value) const {
        auto type = value->getType();
        switch (type->getTypeID()) {
            case llvm::Type::TypeID::IntegerTyID: {
                if (type->isIntegerTy(1)) {
                    auto callee = module.getFunction(".castBoolToString");
                    return builder.CreateCall(callee, {value});
                }

                auto callee = module.getFunction(".castIntToString");
                return builder.CreateCall(callee, {value});
            }
            case llvm::Type::TypeID::FloatTyID: {
                auto callee = module.getFunction(".castFloatToString");
                return builder.CreateCall(callee, {value});
            }
        }

        return value;
    }

    llvm::Type *Codegen::deref(llvm::Type *type) const {
        if (type->isPointerTy()) {
            return type->getPointerElementType();
        }

        return type;
    }

    bool Codegen::isStringType(llvm::Type *type) const {
        type = deref(type);
        return type->isStructTy() && type->getStructName() == Runtime::String::CLASS_NAME;
    }

    llvm::Value *Codegen::compareStrings(llvm::Value *first, llvm::Value *second) const {
        auto callee = module.getFunction(".compareStrings");
        return builder.CreateCall(callee, {first, second});
    }

    llvm::Value *Codegen::negate(llvm::Value *value) const {
        return builder.CreateXor(value, llvm::ConstantInt::getTrue(context));
    }
}
