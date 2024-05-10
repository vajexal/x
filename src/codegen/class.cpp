#include <array>
#include <fmt/core.h>

#include "codegen.h"
#include "runtime/runtime.h"
#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(ClassNode *node) {
        auto &name = node->getName();
        const auto &mangledName = Mangler::mangleClass(name);

        self = &classes[name];

        auto &abstractMethods = node->getAbstractMethods();
        for (auto &[methodName, method]: node->getMethods()) {
            if (abstractMethods.contains(methodName)) {
                throw MethodAlreadyDeclaredException(name, methodName);
            }

            auto fn = method->getFnDef();
            currentFnRetType = method->getFnDef()->getReturnType();
            const auto &fnName = Mangler::mangleMethod(mangledName, fn->getName());
            genFn(fnName, fn->getArgs(), currentFnRetType, fn->getBody(), method->getIsStatic() ? nullptr : &self->type);
        }

        self = nullptr;

        return nullptr;
    }

    llvm::Value *Codegen::gen(FetchPropNode *node) {
        auto obj = node->getObj()->gen(*this);
        auto &propName = node->getName();
        auto [type, ptr] = getProp(obj, node->getObj()->getType(), propName);
        return builder.CreateLoad(mapType(type), ptr, propName);
    }

    llvm::Value *Codegen::gen(FetchStaticPropNode *node) {
        auto [type, ptr] = getStaticProp(node->getClassName(), node->getPropName());
        return builder.CreateLoad(mapType(type), ptr, node->getPropName());
    }

    llvm::Value *Codegen::gen(MethodCallNode *node) {
        auto obj = node->getObj()->gen(*this);
        auto &methodName = node->getName();

        if (methodName == CONSTRUCTOR_FN_NAME) {
            throw MethodNotFoundException(getClassName(node->getObj()->getType()), methodName);
        }

        return callMethod(obj, node->getObj()->getType(), methodName, node->getArgs());
    }

    llvm::Value *Codegen::gen(StaticMethodCallNode *node) {
        return callStaticMethod(node->getClassName(), node->getMethodName(), node->getArgs());
    }

    llvm::Value *Codegen::gen(AssignPropNode *node) {
        if (!isObject(node->getObj()->getType())) {
            throw InvalidObjectAccessException();
        }
        auto obj = node->getObj()->gen(*this);
        auto value = node->getExpr()->gen(*this);
        auto [type, ptr] = getProp(obj, node->getObj()->getType(), node->getName());

        value = castTo(value, node->getExpr()->getType(), type);
        builder.CreateStore(value, ptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignStaticPropNode *node) {
        auto value = node->getExpr()->gen(*this);
        auto [type, ptr] = getStaticProp(node->getClassName(), node->getPropName());

        value = castTo(value, node->getExpr()->getType(), type);
        builder.CreateStore(value, ptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(NewNode *node) {
        auto &classDecl = getClassDecl(node->getName());
        if (classDecl.isAbstract) {
            throw CodegenException("cannot instantiate abstract class " + node->getName());
        }

        auto obj = newObj(classDecl.llvmType);

        initVtable(obj, classDecl);

        auto initFnName = Mangler::mangleHiddenMethod(Mangler::mangleClass(classDecl.name), INIT_FN_NAME);
        if (auto initFn = module.getFunction(initFnName)) {
            builder.CreateCall(initFn, {obj});
        }

        try {
            callMethod(obj, classDecl.type, CONSTRUCTOR_FN_NAME, node->getArgs());
        } catch (const MethodNotFoundException &e) {
            if (!node->getArgs().empty()) {
                throw CodegenException("constructor args mismatch");
            }
        }

        return obj;
    }

    llvm::Value *Codegen::gen(InterfaceNode *node) {
        auto &interfaceDecl = interfaces[node->getName()];

        interfaceDecl.vtableType = genVtable(node, interfaceDecl);
        // {vtable, obj ptr, gc meta}
        interfaceDecl.llvmType->setBody({builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});

        return nullptr;
    }

    std::pair<const Type &, llvm::Value *> Codegen::getProp(llvm::Value *obj, const Type &objType, const std::string &name) {
        if (!objType.is(Type::TypeID::CLASS)) {
            throw InvalidObjectAccessException();
        }

        auto &classDecl = getClassDecl(objType.getClassName());
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto propIt = currentClassDecl->props.find(name);
            if (propIt != currentClassDecl->props.end()) {
                if ((propIt->second.accessModifier == AccessModifier::PROTECTED && !that) ||
                    (propIt->second.accessModifier == AccessModifier::PRIVATE && currentClassDecl != &classDecl)) {
                    throw PropAccessException(currentClassDecl->name, name);
                }

                auto ptr = builder.CreateStructGEP(currentClassDecl->llvmType, obj, propIt->second.pos);
                return {propIt->second.type, ptr};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        throw PropNotFoundException(classDecl.name, name);
    }

    std::pair<const Type &, llvm::Value *> Codegen::getStaticProp(const std::string &className, const std::string &propName) const {
        auto &classDecl = className == SELF_KEYWORD && self ? *self : getClassDecl(className);
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto propIt = currentClassDecl->staticProps.find(propName);
            if (propIt != currentClassDecl->staticProps.end()) {
                if ((propIt->second.accessModifier == AccessModifier::PROTECTED && !self) ||
                    (propIt->second.accessModifier == AccessModifier::PRIVATE && currentClassDecl != &classDecl)) {
                    throw PropAccessException(currentClassDecl->name, propName);
                }

                return {propIt->second.type, propIt->second.var};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        throw PropNotFoundException(className, propName);
    }

    const ClassDecl &Codegen::getClassDecl(const std::string &name) const {
        auto classDecl = classes.find(name);
        if (classDecl == classes.end()) {
            throw ClassNotFoundException(name);
        }
        return classDecl->second;
    }

    const InterfaceDecl *Codegen::findInterfaceDecl(const std::string &name) const {
        auto interfaceIt = interfaces.find(name);
        return interfaceIt != interfaces.cend() ? &interfaceIt->second : nullptr;
    }

    std::string Codegen::getClassName(const Type &type) const {
        switch (type.getTypeID()) {
            case Type::TypeID::CLASS:
                return type.getClassName();
            case Type::TypeID::STRING:
                return Runtime::String::CLASS_NAME;
            case Type::TypeID::ARRAY:
                return Runtime::ArrayRuntime::getClassName(type);
            default:
                throw InvalidTypeException();
        }
    }

    GC::Metadata *Codegen::getTypeGCMeta(const Type &type) {
        auto typeName = getTypeGCMetaKey(type);

        auto metaIt = gcMetaCache.find(typeName);
        if (metaIt != gcMetaCache.cend()) {
            return metaIt->second;
        }

        auto meta = genTypeGCMeta(type);
        if (meta) {
            gcMetaCache[typeName] = meta;
        }

        return meta;
    }

    GC::Metadata *Codegen::genTypeGCMeta(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::STRING:
                return gc.addMeta(GC::NodeType::CLASS, {});
            case Type::TypeID::ARRAY: {
                GC::PointerList pointerList;
                auto containedMeta = getTypeGCMeta(*type.getSubtype());
                if (containedMeta) {
                    pointerList.emplace_back(0, containedMeta);
                }
                return gc.addMeta(GC::NodeType::ARRAY, std::move(pointerList));
            }
            case Type::TypeID::CLASS: {
                if (type.getClassName() == Runtime::Range::CLASS_NAME) {
                    return gc.addMeta(GC::NodeType::CLASS, {});
                }

                if (isInterfaceType(type)) {
                    return gc.addMeta(GC::NodeType::INTERFACE, {});
                }

                auto &classDecl = getClassDecl(type.getClassName());
                return classDecl.meta;
            }
            default:
                return nullptr;
        }
    }

    std::string Codegen::getTypeGCMetaKey(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::CLASS:
                return type.getClassName();
            case Type::TypeID::STRING:
                return Runtime::String::CLASS_NAME;
            case Type::TypeID::ARRAY:
                return Runtime::ArrayRuntime::CLASS_NAME;
            default:
                return "";
        }
    }

    llvm::Value *Codegen::getGCMetaValue(const Type &type) {
        auto meta = getTypeGCMeta(type);
        return meta ? builder.CreateIntToPtr(builder.getInt64((uint64_t)meta), builder.getPtrTy()) : nullptr;
    }

    bool Codegen::isObject(const Type &type) const {
        return type.isOneOf(Type::TypeID::CLASS, Type::TypeID::STRING, Type::TypeID::ARRAY);
    }

    bool Codegen::isInterfaceType(const Type &type) const {
        if (!type.is(Type::TypeID::CLASS)) {
            return false;
        }

        return interfaces.contains(type.getClassName());
    }

    llvm::StructType *Codegen::genVtable(ClassNode *classNode, ClassDecl &classDecl) {
        const auto &classMangledName = Mangler::mangleClass(classNode->getName());
        auto &classMethods = classNode->getMethods();
        const auto &virtualMethods = compilerRuntime.virtualMethods.at(classNode->getName());
        std::vector<llvm::Type *> vtableProps;
        vtableProps.reserve(virtualMethods.size());
        uint64_t vtablePos = 0;

        for (auto &methodName: virtualMethods) {
            auto methodDef = classMethods.at(methodName);
            auto fnType = genFnType(methodDef->getFnDef()->getArgs(), methodDef->getFnDef()->getReturnType(), &classDecl.type);
            vtableProps.push_back(builder.getPtrTy());
            classDecl.methods[methodName] = {methodDef->getAccessModifier(), fnType, true, vtablePos++};
        }

        return llvm::StructType::create(context, vtableProps, "vtable." + classMangledName);
    }

    llvm::StructType *Codegen::genVtable(InterfaceNode *node, InterfaceDecl &interfaceDecl) {
        const auto &interfaceMangledName = Mangler::mangleInterface(node->getName());
        auto &interfaceMethods = compilerRuntime.interfaceMethods.at(node->getName());
        std::vector<llvm::Type *> vtableProps;
        vtableProps.reserve(interfaceMethods.size());
        uint64_t vtablePos = 0;

        for (auto &[methodName, methodDecl]: interfaceMethods) {
            auto fnType = genFnType(methodDecl->getFnDecl()->getArgs(), methodDecl->getFnDecl()->getReturnType(), &interfaceDecl.type);
            vtableProps.push_back(builder.getPtrTy());
            interfaceDecl.methods[methodName] = {methodDecl->getAccessModifier(), fnType, true, vtablePos++};
        }

        return llvm::StructType::create(context, vtableProps, "vtable." + interfaceMangledName);
    }

    void Codegen::initVtable(llvm::Value *obj, const ClassDecl &classDecl) {
        auto currentClassDecl = &classDecl;

        while (currentClassDecl) {
            if (currentClassDecl->vtableType) {
                auto vtableName = fmt::format("{}.{}", currentClassDecl->vtableType->getName().str(), classDecl.name);
                auto vtable = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(vtableName, currentClassDecl->vtableType));

                if (!vtable->hasInitializer()) {
                    std::vector<llvm::Constant *> funcs;

                    for (const auto &it: currentClassDecl->methods) {
                        if (it.second.isVirtual) {
                            auto fn = simpleFindMethod(classDecl, it.first);
                            // just in case
                            if (!fn) {
                                throw MethodNotFoundException(classDecl.name, it.first);
                            }
                            funcs.push_back(fn);
                        }
                    }

                    vtable->setInitializer(llvm::ConstantStruct::get(currentClassDecl->vtableType, funcs));
                }

                auto vtablePos = currentClassDecl->parent ? 1 : 0;
                auto vtablePtr = builder.CreateStructGEP(currentClassDecl->llvmType, obj, vtablePos);
                builder.CreateStore(vtable, vtablePtr);
            }

            currentClassDecl = currentClassDecl->parent;
        }
    }

    void Codegen::initInterfaceVtable(llvm::Value *obj, const Type &objType, llvm::Value *interface, const InterfaceDecl &interfaceDecl) {
        auto &classDecl = getClassDecl(objType.getClassName());
        auto vtableName = fmt::format("{}.{}", interfaceDecl.vtableType->getName().str(), classDecl.name);
        auto vtable = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(vtableName, interfaceDecl.vtableType));

        if (!vtable->hasInitializer()) {
            std::vector<llvm::Constant *> funcs;
            funcs.reserve(interfaceDecl.methods.size());

            for (const auto &it: interfaceDecl.methods) {
                auto fn = simpleFindMethod(classDecl, it.first);
                // just in case
                if (!fn) {
                    throw MethodNotFoundException(classDecl.name, it.first);
                }
                funcs.push_back(fn);
            }

            vtable->setInitializer(llvm::ConstantStruct::get(interfaceDecl.vtableType, funcs));
        }

        auto vtablePtr = builder.CreateStructGEP(interfaceDecl.llvmType, interface, 0);
        builder.CreateStore(vtable, vtablePtr);
    }

    llvm::Function *Codegen::getInternalConstructor(const std::string &mangledClassName) const {
        return module.getFunction(Mangler::mangleInternalMethod(mangledClassName, CONSTRUCTOR_FN_NAME));
    }

    llvm::Value *Codegen::callMethod(llvm::Value *obj, const Type &objType, const std::string &methodName, const ExprList &args) {
        auto [callee, fnType] = findMethod(obj, objType, methodName);
        if (!callee.getCallee()) {
            throw MethodNotFoundException(getClassName(objType), methodName);
        }

        if (objType.is(Type::TypeID::CLASS)) {
            auto interfaceDecl = findInterfaceDecl(objType.getClassName());
            if (interfaceDecl) {
                // get "this" from interface object
                auto objPtr = builder.CreateStructGEP(interfaceDecl->llvmType, obj, 1);
                obj = builder.CreateLoad(builder.getPtrTy(), objPtr);
            }
        }

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size() + 1);
        llvmArgs.push_back(obj);
        for (auto i = 0; i < args.size(); i++) {
            auto val = args[i]->gen(*this);
            val = castTo(val, args[i]->getType(), fnType->args[i]);
            llvmArgs.push_back(val);
        }

        return builder.CreateCall(callee, llvmArgs);
    }

    llvm::Value *Codegen::callStaticMethod(const std::string &className, const std::string &methodName, const ExprList &args) {
        auto &classDecl = className == SELF_KEYWORD && self ? *self : getClassDecl(className);
        auto [callee, fnType] = findMethod(nullptr, classDecl.type, methodName);
        if (!callee.getCallee()) {
            throw MethodNotFoundException(className, methodName);
        }

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size());
        for (auto i = 0; i < args.size(); i++) {
            auto val = args[i]->gen(*this);
            val = castTo(val, args[i]->getType(), fnType->args[i]);
            llvmArgs.push_back(val);
        }

        return builder.CreateCall(callee, llvmArgs);
    }

    std::tuple<llvm::FunctionCallee, FnType *> Codegen::findMethod(llvm::Value *obj, const Type &objType, const std::string &methodName) {
        if (objType.isOneOf(Type::TypeID::STRING, Type::TypeID::ARRAY)) {
            const auto &name = Mangler::mangleInternalMethod(getClassName(objType), methodName);
            auto fn = module.getFunction(name);
            auto &className = objType.is(Type::TypeID::STRING) ? Runtime::String::CLASS_NAME : Runtime::ArrayRuntime::CLASS_NAME;
            return {llvm::FunctionCallee(fn->getFunctionType(), fn), &compilerRuntime.classMethodTypes.at(className).at(methodName)};
        }

        auto interfaceDecl = findInterfaceDecl(objType.getClassName());
        if (interfaceDecl) {
            auto methodIt = interfaceDecl->methods.find(methodName);
            if (methodIt != interfaceDecl->methods.cend()) {
                // get vtable
                auto vtablePtr = builder.CreateStructGEP(interfaceDecl->llvmType, obj, 0);
                auto vtable = builder.CreateLoad(builder.getPtrTy(), vtablePtr);
                // get method
                auto methodPtr = builder.CreateStructGEP(interfaceDecl->vtableType, vtable, methodIt->second.vtablePos);
                auto methodType = interfaceDecl->vtableType->getElementType(methodIt->second.vtablePos);
                auto method = builder.CreateLoad(methodType, methodPtr);

                return {llvm::FunctionCallee(methodIt->second.type, method), &compilerRuntime.classMethodTypes.at(interfaceDecl->name).at(methodName)};
            }
        }

        auto &classDecl = getClassDecl(objType.getClassName());
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto methodIt = currentClassDecl->methods.find(methodName);
            if (methodIt != currentClassDecl->methods.cend()) {
                if ((methodIt->second.accessModifier == AccessModifier::PROTECTED && !that) ||
                    (methodIt->second.accessModifier == AccessModifier::PRIVATE && currentClassDecl != &classDecl)) {
                    throw MethodAccessException(currentClassDecl->name, methodName);
                }

                if (methodIt->second.isVirtual) {
                    // get vtable
                    auto vtablePos = currentClassDecl->parent ? 1 : 0;
                    auto vtablePtr = builder.CreateStructGEP(currentClassDecl->llvmType, obj, vtablePos);
                    auto vtable = builder.CreateLoad(builder.getPtrTy(), vtablePtr);
                    // get method
                    auto methodPtr = builder.CreateStructGEP(currentClassDecl->vtableType, vtable, methodIt->second.vtablePos);
                    auto methodType = currentClassDecl->vtableType->getElementType(methodIt->second.vtablePos);
                    auto method = builder.CreateLoad(methodType, methodPtr);
                    return {llvm::FunctionCallee(methodIt->second.type, method), &compilerRuntime.classMethodTypes.at(currentClassDecl->name).at(methodName)};
                }

                auto fn = module.getFunction(Mangler::mangleMethod(currentClassDecl->llvmType->getName().str(), methodName));
                if (fn) {
                    return {llvm::FunctionCallee(fn->getFunctionType(), fn), &compilerRuntime.classMethodTypes.at(currentClassDecl->name).at(methodName)};
                }
            }

            currentClassDecl = currentClassDecl->parent;
        }

        return {llvm::FunctionCallee(), nullptr};
    }

    llvm::Function *Codegen::simpleFindMethod(const ClassDecl &classDecl, const std::string &methodName) const {
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            const auto &className = currentClassDecl->llvmType->getName().str();
            auto fn = module.getFunction(Mangler::mangleMethod(className, methodName));
            if (fn) {
                return fn;
            }

            currentClassDecl = currentClassDecl->parent;
        }

        return nullptr;
    }
}
