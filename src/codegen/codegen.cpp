#include "codegen.h"

#include <ranges>

#include "utils.h"

namespace X::Codegen {
    void Codegen::genProgram(TopStatementListNode *node) {
        declInterfaces(node);
        declClasses(node);
        declMethods(node);
        // declare props here, so we can calc class size (for allocating objects)
        declProps(node);
        declFuncs(node);
        declGlobals(node);

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
            if (llvm::isa<DeclNode>(child)) {
                continue; // we already declared globals
            }

            child->gen(*this);
        }

        return nullptr;
    }

    llvm::Type *Codegen::mapType(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return builder.getInt64Ty();
            case Type::TypeID::FLOAT:
                return builder.getDoubleTy();
            case Type::TypeID::BOOL:
                return builder.getInt1Ty();
            case Type::TypeID::STRING:
                return builder.getPtrTy();
            case Type::TypeID::ARRAY: {
                auto _ = getArrayForType(type); // need to generate array type
                return builder.getPtrTy();
            }
            case Type::TypeID::VOID:
                return builder.getVoidTy();
            case Type::TypeID::CLASS: {
                return builder.getPtrTy();
            }
            default:
                throw InvalidTypeException();
        }
    }

    llvm::Constant *Codegen::getDefaultValue(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return builder.getInt64(0);
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(builder.getDoubleTy(), 0);
            case Type::TypeID::BOOL:
                return builder.getFalse();
            case Type::TypeID::STRING:
            case Type::TypeID::ARRAY:
            case Type::TypeID::CLASS:
                return llvm::ConstantPointerNull::get(builder.getPtrTy());
            default:
                throw InvalidTypeException();
        }
    }

    llvm::Value *Codegen::createDefaultValue(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return builder.getInt64(0);
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(builder.getDoubleTy(), 0);
            case Type::TypeID::BOOL:
                return builder.getFalse();
            case Type::TypeID::STRING:
                return builder.CreateCall(module.getFunction(mangler->mangleInternalFunction("createEmptyString")));
            case Type::TypeID::ARRAY: {
                auto arrType = getArrayForType(type);
                auto arr = newObj(arrType);
                auto len = builder.getInt64(0);
                builder.CreateCall(getInternalConstructor(arrType->getName().str()), {arr, len});
                return arr;
            }
            default:
                throw InvalidTypeException();
        }
    }

    std::pair<Type, llvm::Value *> Codegen::getVar(std::string &name) {
        // most nested scope will be last, so search in reverse order
        for (auto &vars: std::ranges::reverse_view(varScopes)) {
            auto var = vars.find(name);
            if (var != vars.cend()) {
                return {var->second.type, var->second.value};
            }
        }

        if (that) {
            try {
                return getProp(that->value, that->type, name);
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
        auto allocSize = getTypeSize(module, type);
        return gcAlloc(allocSize);
    }

    std::tuple<llvm::Value *, Type, llvm::Value *, Type> Codegen::upcast(llvm::Value *a, Type aType, llvm::Value *b, Type bType) const {
        if (aType.is(Type::TypeID::FLOAT) && bType.is(Type::TypeID::INT)) {
            return {a, aType, builder.CreateSIToFP(b, builder.getDoubleTy()), Type::scalar(Type::TypeID::FLOAT)};
        }

        if (aType.is(Type::TypeID::INT) && bType.is(Type::TypeID::FLOAT)) {
            return {builder.CreateSIToFP(a, builder.getDoubleTy()), Type::scalar(Type::TypeID::FLOAT), b, bType};
        }

        return {a, aType, b, bType};
    }

    std::tuple<llvm::Value *, Type, llvm::Value *, Type> Codegen::forceUpcast(llvm::Value *a, Type aType, llvm::Value *b, Type bType) const {
        if (aType.is(Type::TypeID::INT)) {
            a = builder.CreateSIToFP(a, builder.getDoubleTy());
            aType = Type::scalar(Type::TypeID::FLOAT);
        }

        if (bType.is(Type::TypeID::INT)) {
            b = builder.CreateSIToFP(b, builder.getDoubleTy());
            bType = Type::scalar(Type::TypeID::FLOAT);
        }

        return {a, aType, b, bType};
    }

    llvm::Value *Codegen::downcastToBool(llvm::Value *value, const Type &type) const {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return builder.CreateICmpNE(value, builder.getInt64(0));
            case Type::TypeID::FLOAT:
                return builder.CreateFCmpONE(value, llvm::ConstantFP::get(builder.getDoubleTy(), 0));
            case Type::TypeID::BOOL:
                return value;
            case Type::TypeID::STRING: {
                const auto &stringIsEmptyFnName = mangler->mangleInternalMethod(Runtime::String::CLASS_NAME, "isEmpty");
                auto stringIsEmptyFn = module.getFunction(stringIsEmptyFnName);
                auto val = builder.CreateCall(stringIsEmptyFn, {value});
                return negate(val);
            }
            case Type::TypeID::ARRAY: {
                const auto &arrayClassName = Runtime::ArrayRuntime::getClassName(type);
                const auto &arrayIsEmptyFnName = mangler->mangleInternalMethod(arrayClassName, "isEmpty");
                auto arrayIsEmptyFn = module.getFunction(arrayIsEmptyFnName);
                auto val = builder.CreateCall(arrayIsEmptyFn, {value});
                return negate(val);
            }
            default:
                throw InvalidTypeException();
        }
    }

    bool Codegen::instanceof(const Type &instanceType, const Type &type) const {
        auto &currentClassDecl = getClassDecl(instanceType.getClassName());
        auto expectedInterfaceDecl = findInterfaceDecl(type.getClassName());
        if (expectedInterfaceDecl) {
            return compilerRuntime->implementedInterfaces[currentClassDecl.name].contains(expectedInterfaceDecl->name);
        }

        auto &expectedClassDecl = getClassDecl(type.getClassName());
        return compilerRuntime->extendedClasses[currentClassDecl.name].contains(expectedClassDecl.name);
    }

    llvm::Value *Codegen::castTo(llvm::Value *value, const Type &type, const Type &expectedType) {
        if (type == expectedType) {
            return value;
        }

        if (expectedType.is(Type::TypeID::FLOAT) && type.is(Type::TypeID::INT)) {
            return builder.CreateSIToFP(value, builder.getDoubleTy());
        }

        if (type.is(Type::TypeID::CLASS) && expectedType.is(Type::TypeID::CLASS) && instanceof(type, expectedType)) {
            auto interfaceDecl = findInterfaceDecl(expectedType.getClassName());
            return interfaceDecl ?
                   instantiateInterface(value, type, *interfaceDecl) :
                   value;
        }

        return value;
    }

    llvm::Value *Codegen::instantiateInterface(llvm::Value *value, const Type &type, const InterfaceDecl &interfaceDecl) {
        auto interface = newObj(interfaceDecl.llvmType);

        initInterfaceVtable(value, type, interface, interfaceDecl);

        // set obj ptr
        auto objPtr = builder.CreateStructGEP(interfaceDecl.llvmType, interface, 1);
        builder.CreateStore(value, objPtr);

        // set gc meta
        auto meta = getGCMetaValue(type);
        auto metaPtr = builder.CreateStructGEP(interfaceDecl.llvmType, interface, 2);
        builder.CreateStore(meta, metaPtr);

        return interface;
    }

    llvm::Value *Codegen::compareStrings(llvm::Value *first, llvm::Value *second) const {
        auto compareStringsFn = module.getFunction(mangler->mangleInternalFunction("compareStrings"));
        return builder.CreateCall(compareStringsFn, {first, second});
    }

    llvm::Value *Codegen::negate(llvm::Value *value) const {
        return builder.CreateXor(value, builder.getTrue());
    }

    llvm::StructType *Codegen::getArrayForType(const Type &arrType) {
        if (!arrType.is(Type::TypeID::ARRAY)) {
            throw InvalidTypeException();
        }

        auto &subtype = *arrType.getSubtype();

        if (subtype.is(Type::TypeID::ARRAY)) {
            throw CodegenException("multidimensional arrays are not supported");
        }

        const auto &arrayClassName = Runtime::ArrayRuntime::getClassName(arrType);
        auto arrayType = llvm::StructType::getTypeByName(context, arrayClassName);
        if (!arrayType) {
            // gen array subtype
            return arrayRuntime->add(arrType, mapType(subtype));
        }
        return arrayType;
    }

    void Codegen::fillArray(llvm::Value *arr, const Type &type, const std::vector<llvm::Value *> &values) {
        const auto &arrayClassName = Runtime::ArrayRuntime::getClassName(type);
        auto arrSetFn = module.getFunction(mangler->mangleInternalMethod(arrayClassName, "set[]"));
        if (!arrSetFn) {
            throw InvalidArrayAccessException();
        }

        for (auto i = 0; i < values.size(); i++) {
            builder.CreateCall(arrSetFn, {arr, builder.getInt64(i), values[i]});
        }
    }

    void Codegen::addSymbol(const std::string &symbol) {
        auto [_, inserted] = symbols.insert(symbol);
        if (!inserted) {
            throw SymbolAlreadyExistsException(symbol);
        }
    }
}
