#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    void Codegen::genProgram(TopStatementListNode *node) {
        declInterfaces(node);
        declClasses(node);
        declMethods(node);
        // declare props here, so we can calc class size (for allocating objects)
        declProps(node);
        declFuncs(node);

        gen(node);
    }

    llvm::Value *Codegen::gen(Node *node) {
        throw CodegenException("can't gen node");
    }

    llvm::Value *Codegen::gen(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            child->gen(*this);

            if (child->isTerminate()) {
                break;
            }
        }

        return nullptr;
    }

    llvm::Value *Codegen::gen(TopStatementListNode *node) {
        for (auto child: node->getChildren()) {
            child->gen(*this);
        }

        return nullptr;
    }

    llvm::Type *Codegen::mapType(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::FLOAT:
                return llvm::Type::getDoubleTy(context);
            case Type::TypeID::BOOL:
                return llvm::Type::getInt1Ty(context);
            case Type::TypeID::STRING:
                return llvm::StructType::getTypeByName(context, Runtime::String::CLASS_NAME)->getPointerTo();
            case Type::TypeID::ARRAY:
                return getArrayForType(type.getSubtype())->getPointerTo();
            case Type::TypeID::VOID:
                return llvm::Type::getVoidTy(context);
            case Type::TypeID::CLASS: {
                auto &className = type.getClassName();
                auto &classDecl = getClassDecl(mangler.mangleClass(className));
                return classDecl.type->getPointerTo();
            }
            default:
                throw InvalidTypeException();
        }
    }

    llvm::Type *Codegen::mapArgType(const Type &type) {
        if (type.getTypeID() == Type::TypeID::CLASS) {
            auto interfaceDecl = findInterfaceDecl(mangler.mangleInterface(type.getClassName()));
            if (interfaceDecl) {
                return interfaceDecl->type->getPointerTo();
            }
        }

        return mapType(type);
    }

    llvm::Constant *Codegen::getDefaultValue(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0);
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 0);
            case Type::TypeID::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            case Type::TypeID::STRING:
                // todo optimize
                return llvm::ConstantPointerNull::get(llvm::StructType::getTypeByName(context, Runtime::String::CLASS_NAME)->getPointerTo());
            case Type::TypeID::ARRAY:
                return llvm::ConstantPointerNull::get(getArrayForType(type.getSubtype())->getPointerTo());
            default:
                throw InvalidTypeException();
        }
    }

    llvm::Value *Codegen::createDefaultValue(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0);
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 0);
            case Type::TypeID::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            case Type::TypeID::STRING:
                return builder.CreateCall(module.getFunction(mangler.mangleInternalFunction("createEmptyString")));
            case Type::TypeID::ARRAY: {
                auto arrType = getArrayForType(type.getSubtype());
                auto arr = createAlloca(arrType);
                auto len = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0);
                builder.CreateCall(getInternalConstructor(arrType->getName().str()), {arr, len});
                return arr;
            }
            default:
                throw InvalidTypeException();
        }
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getVar(std::string &name) const {
        auto var = namedValues.find(name);
        if (var != namedValues.end()) {
            return {var->second->getAllocatedType(), var->second};
        }

        if (that) {
            try {
                return getProp(that, name);
            } catch (const PropNotFoundException &e) {}
        }

        if (self) {
            try {
                return getStaticProp(self->name, name);
            } catch (const PropNotFoundException &e) {}
        }

        throw VarNotFoundException(name);
    }

    llvm::AllocaInst *Codegen::createAlloca(llvm::Type *type, const std::string &name) const {
        auto fn = builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        return tmpBuilder.CreateAlloca(type, nullptr, name);
    }

    // allocates object on heap
    llvm::Value *Codegen::newObj(llvm::StructType *type) {
        auto typeSize = module.getDataLayout().getTypeAllocSize(type);
        if (typeSize.isScalable()) {
            throw CodegenException("can't calc obj size");
        }
        auto allocSize = builder.getInt64(typeSize.getFixedSize());

        auto objPtr = gcAlloc(allocSize);
        auto obj = builder.CreateBitCast(objPtr, type->getPointerTo());

        return obj;
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::upcast(llvm::Value *a, llvm::Value *b) const {
        if (a->getType()->isDoubleTy() && b->getType()->isIntegerTy(INTEGER_BIT_WIDTH)) {
            return {a, builder.CreateSIToFP(b, llvm::Type::getDoubleTy(context))};
        }

        if (a->getType()->isIntegerTy(INTEGER_BIT_WIDTH) && b->getType()->isDoubleTy()) {
            return {builder.CreateSIToFP(a, llvm::Type::getDoubleTy(context)), b};
        }

        return {a, b};
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::forceUpcast(llvm::Value *a, llvm::Value *b) const {
        if (a->getType()->isIntegerTy(INTEGER_BIT_WIDTH)) {
            a = builder.CreateSIToFP(a, llvm::Type::getDoubleTy(context));
        }

        if (b->getType()->isIntegerTy(INTEGER_BIT_WIDTH)) {
            b = builder.CreateSIToFP(b, llvm::Type::getDoubleTy(context));
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
            case llvm::Type::TypeID::DoubleTyID:
                return builder.CreateFCmpONE(value, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), 0));
            default:
                if (Runtime::String::isStringType(value->getType())) {
                    const auto &stringIsEmptyFnName = mangler.mangleInternalMethod(Runtime::String::CLASS_NAME, "isEmpty");
                    auto stringIsEmptyFn = module.getFunction(stringIsEmptyFnName);
                    auto val = builder.CreateCall(stringIsEmptyFn, {value});
                    return negate(val);
                } else if (Runtime::Array::isArrayType(value->getType())) {
                    auto arrType = deref(value->getType());
                    const auto &arrayIsEmptyFnName = mangler.mangleInternalMethod(arrType->getStructName().str(), "isEmpty");
                    auto arrayIsEmptyFn = module.getFunction(arrayIsEmptyFnName);
                    auto val = builder.CreateCall(arrayIsEmptyFn, {value});
                    return negate(val);
                }
                throw InvalidTypeException();
        }
    }

    llvm::Value *Codegen::castToString(llvm::Value *value) const {
        auto type = value->getType();
        switch (type->getTypeID()) {
            case llvm::Type::TypeID::IntegerTyID: {
                if (type->isIntegerTy(1)) {
                    auto castBoolToStringFn = module.getFunction(mangler.mangleInternalFunction("castBoolToString"));
                    return builder.CreateCall(castBoolToStringFn, {value});
                }

                auto castIntToStringFn = module.getFunction(mangler.mangleInternalFunction("castIntToString"));
                return builder.CreateCall(castIntToStringFn, {value});
            }
            case llvm::Type::TypeID::DoubleTyID: {
                auto castFloatToStringFn = module.getFunction(mangler.mangleInternalFunction("castFloatToString"));
                return builder.CreateCall(castFloatToStringFn, {value});
            }
        }

        return value;
    }

    bool Codegen::instanceof(llvm::StructType *instanceType, llvm::StructType *type) const {
        const auto &expectedTypeName = type->getName().str();
        auto &currentClassDecl = getClassDecl(instanceType->getStructName().str());
        auto expectedInterfaceDecl = findInterfaceDecl(expectedTypeName);
        if (expectedInterfaceDecl) {
            return compilerRuntime.implementedInterfaces[currentClassDecl.name].contains(expectedInterfaceDecl->name);
        }

        auto &expectedClassDecl = getClassDecl(expectedTypeName);
        return compilerRuntime.extendedClasses[currentClassDecl.name].contains(expectedClassDecl.name);
    }

    llvm::Value *Codegen::castTo(llvm::Value *value, llvm::Type *expectedType) {
        if (value->getType() == expectedType) {
            return value;
        }

        if (expectedType->isDoubleTy() && value->getType()->isIntegerTy(INTEGER_BIT_WIDTH)) {
            return builder.CreateSIToFP(value, expectedType);
        }

        auto type = deref(value->getType());
        auto expectedClassType = deref(expectedType);
        if (type->isStructTy() && expectedClassType->isStructTy() &&
            instanceof(llvm::cast<llvm::StructType>(type), llvm::cast<llvm::StructType>(expectedClassType))) {
            auto interfaceDecl = findInterfaceDecl(expectedClassType->getStructName().str());
            return interfaceDecl ?
                   instantiateInterface(value, *interfaceDecl) :
                   builder.CreateBitCast(value, expectedType);
        }

        return value;
    }

    llvm::Value *Codegen::instantiateInterface(llvm::Value *value, const InterfaceDecl &interfaceDecl) {
        auto interface = newObj(interfaceDecl.type);

        initInterfaceVtable(value, interface);

        // set obj ptr
        auto objPtr = builder.CreateStructGEP(interfaceDecl.type, interface, 1);
        auto valueVoidPtr = builder.CreateBitCast(value, builder.getInt8PtrTy());
        builder.CreateStore(valueVoidPtr, objPtr);

        // set class id
        const auto &valueClassName = deref(value->getType())->getStructName().str();
        auto classId = getClassIdByMangledName(valueClassName);
        auto classIdPtr = builder.CreateStructGEP(interfaceDecl.type, interface, 2);
        builder.CreateStore(builder.getInt64(classId), classIdPtr);

        return interface;
    }

    llvm::Value *Codegen::compareStrings(llvm::Value *first, llvm::Value *second) const {
        auto compareStringsFn = module.getFunction(mangler.mangleInternalFunction("compareStrings"));
        return builder.CreateCall(compareStringsFn, {first, second});
    }

    llvm::Value *Codegen::negate(llvm::Value *value) const {
        return builder.CreateXor(value, llvm::ConstantInt::getTrue(context));
    }

    llvm::StructType *Codegen::getArrayForType(const Type *type) {
        if (type->getTypeID() == Type::TypeID::ARRAY) {
            throw CodegenException("multidimensional arrays are forbidden");
        }

        const auto &arrayClassName = Runtime::Array::getClassName(type);
        // todo cache
        auto arrayType = llvm::StructType::getTypeByName(context, arrayClassName);
        if (!arrayType) {
            // gen array type
            auto subtype = mapType(*type);
            return arrayRuntime.add(subtype);
        }
        return arrayType;
    }

    void Codegen::fillArray(llvm::Value *arr, const std::vector<llvm::Value *> &values) {
        auto arrType = deref(arr->getType());
        auto arrSetFn = module.getFunction(mangler.mangleInternalMethod(arrType->getStructName().str(), "set[]"));
        if (!arrSetFn) {
            throw InvalidArrayAccessException();
        }

        for (auto i = 0; i < values.size(); i++) {
            auto index = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), i);
            builder.CreateCall(arrSetFn, {arr, index, values[i]});
        }
    }

    void Codegen::addSymbol(const std::string &symbol) {
        auto [_, inserted] = symbols.insert(symbol);
        if (!inserted) {
            throw SymbolAlreadyExistsException(symbol);
        }
    }
}
