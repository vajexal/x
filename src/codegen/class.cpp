#include <array>
#include <fmt/core.h>

#include "codegen.h"
#include "runtime/runtime.h"
#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(ClassNode *node) {
        auto &name = node->getName();
        const auto &mangledName = mangler.mangleClass(name);

        self = &classes[mangledName];

        auto &abstractMethods = node->getAbstractMethods();
        for (auto &[methodName, method]: node->getMethods()) {
            if (abstractMethods.contains(methodName)) {
                throw MethodAlreadyDeclaredException(name, methodName);
            }

            auto fn = method->getFnDef();
            auto isConstructor = methodName == CONSTRUCTOR_FN_NAME;
            if (isConstructor) {
                checkConstructor(method, name);
            }
            const auto &fnName = mangler.mangleMethod(mangledName, fn->getName());
            genFn(fnName, fn->getArgs(), fn->getReturnType(), fn->getBody(), method->getIsStatic() ? nullptr : self->type);
        }

        self->methods[CONSTRUCTOR_FN_NAME] = {AccessModifier::PUBLIC, false};

        self = nullptr;

        return nullptr;
    }

    llvm::Value *Codegen::gen(FetchPropNode *node) {
        auto obj = node->getObj()->gen(*this);
        if (!isObject(obj)) {
            throw InvalidObjectAccessException();
        }
        auto &propName = node->getName();
        auto [type, ptr] = getProp(obj, propName);
        return builder.CreateLoad(type, ptr, propName);
    }

    llvm::Value *Codegen::gen(FetchStaticPropNode *node) {
        auto [type, ptr] = getStaticProp(node->getClassName(), node->getPropName());
        return builder.CreateLoad(type, ptr, node->getPropName());
    }

    llvm::Value *Codegen::gen(MethodCallNode *node) {
        auto obj = node->getObj()->gen(*this);
        if (!isObject(obj)) {
            throw InvalidObjectAccessException();
        }

        auto &methodName = node->getName();

        if (methodName == CONSTRUCTOR_FN_NAME) {
            throw MethodNotFoundException(getClassName(obj), methodName);
        }

        return callMethod(obj, methodName, node->getArgs());
    }

    llvm::Value *Codegen::gen(StaticMethodCallNode *node) {
        return callStaticMethod(node->getClassName(), node->getMethodName(), node->getArgs());
    }

    llvm::Value *Codegen::gen(AssignPropNode *node) {
        auto obj = node->getObj()->gen(*this);
        if (!isObject(obj)) {
            throw InvalidObjectAccessException();
        }

        auto value = node->getExpr()->gen(*this);
        auto [type, ptr] = getProp(obj, node->getName());

        value = castTo(value, type);
        builder.CreateStore(value, ptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignStaticPropNode *node) {
        auto value = node->getExpr()->gen(*this);
        auto [type, ptr] = getStaticProp(node->getClassName(), node->getPropName());

        value = castTo(value, type);
        builder.CreateStore(value, ptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(NewNode *node) {
        const auto &mangledClassName = mangler.mangleClass(node->getName());
        auto &classDecl = getClassDecl(mangledClassName);
        if (classDecl.isAbstract) {
            throw CodegenException("cannot instantiate abstract class " + node->getName());
        }

        auto obj = newObj(classDecl.type);

        try {
            callMethod(obj, CONSTRUCTOR_FN_NAME, node->getArgs());
        } catch (const MethodNotFoundException &e) {
            if (!node->getArgs().empty()) {
                throw CodegenException("constructor args mismatch");
            }
        }

        initVtable(obj);

        return obj;
    }

    llvm::Value *Codegen::gen(InterfaceNode *node) {
        const auto &mangledName = mangler.mangleInterface(node->getName());
        auto &interfaceDecl = interfaces[mangledName];
        auto interface = interfaceDecl.type;

        auto vtableType = genVtable(node, interface, interfaceDecl);
        interfaceDecl.vtableType = vtableType;
        // {vtable, obj ptr, gc meta}
        interface->setBody({vtableType->getPointerTo(), builder.getInt8PtrTy(), builder.getInt8PtrTy()});

        return nullptr;
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getProp(llvm::Value *obj, const std::string &name) const {
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw InvalidObjectAccessException();
        }

        auto &classDecl = getClassDecl(type->getStructName().str());
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto propIt = currentClassDecl->props.find(name);
            if (propIt != currentClassDecl->props.end()) {
                if ((propIt->second.accessModifier == AccessModifier::PROTECTED && !that) ||
                    (propIt->second.accessModifier == AccessModifier::PRIVATE && currentClassDecl != &classDecl)) {
                    throw PropAccessException(currentClassDecl->name, name);
                }

                auto currentObj = currentClassDecl == &classDecl ? obj : builder.CreateBitCast(obj, currentClassDecl->type->getPointerTo());
                auto ptr = builder.CreateStructGEP(currentClassDecl->type, currentObj, propIt->second.pos);
                return {propIt->second.type, ptr};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        throw PropNotFoundException(classDecl.name, name);
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getStaticProp(const std::string &className, const std::string &propName) const {
        auto &classDecl = className == SELF_KEYWORD && self ? *self : getClassDecl(mangler.mangleClass(className));
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto propIt = currentClassDecl->staticProps.find(propName);
            if (propIt != currentClassDecl->staticProps.end()) {
                if ((propIt->second.accessModifier == AccessModifier::PROTECTED && !self) ||
                    (propIt->second.accessModifier == AccessModifier::PRIVATE && currentClassDecl != &classDecl)) {
                    throw PropAccessException(currentClassDecl->name, propName);
                }

                return {propIt->second.var->getType()->getPointerElementType(), propIt->second.var};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        throw PropNotFoundException(className, propName);
    }

    const ClassDecl &Codegen::getClassDecl(const std::string &mangledName) const {
        auto classDecl = classes.find(mangledName);
        if (classDecl == classes.end()) {
            throw ClassNotFoundException(mangler.unmangleClass(mangledName));
        }
        return classDecl->second;
    }

    const InterfaceDecl *Codegen::findInterfaceDecl(const std::string &mangledName) const {
        auto interfaceIt = interfaces.find(mangledName);
        return interfaceIt != interfaces.cend() ? &interfaceIt->second : nullptr;
    }

    std::string Codegen::getClassName(llvm::Value *obj) const {
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw InvalidTypeException();
        }
        return std::move(mangler.unmangleClass(type->getStructName().str()));
    }

    GC::Metadata *Codegen::getTypeGCMeta(llvm::Type *type) {
        type = deref(type);
        if (!type->isStructTy()) {
            return nullptr;
        }

        const auto &typeName = type->getStructName().str();
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

    GC::Metadata *Codegen::genTypeGCMeta(llvm::Type *type) {
        if (Runtime::String::isStringType(type) || Runtime::Range::isRangeType(type)) {
            return gc.addMeta(GC::NodeType::CLASS, {});
        } else if (Runtime::Array::isArrayType(type)) {
            GC::PointerList pointerList;
            auto containedType = arrayRuntime.getContainedType(llvm::cast<llvm::StructType>(type));
            auto containedMeta = getTypeGCMeta(containedType);
            if (containedMeta) {
                pointerList.emplace_back(0, containedMeta);
            }
            return gc.addMeta(GC::NodeType::ARRAY, std::move(pointerList));
        } else if (isInterfaceType(type)) {
            return gc.addMeta(GC::NodeType::INTERFACE, {});
        } else if (isClassType(type)) {
            auto &classDecl = getClassDecl(type->getStructName().str());
            return classDecl.meta;
        } else {
            return nullptr;
        }
    }

    llvm::Value *Codegen::getValueGCMeta(llvm::Value *value) {
        auto meta = getTypeGCMeta(value->getType());
        return meta ? builder.CreateIntToPtr(builder.getInt64((uint64_t)meta), builder.getInt8PtrTy()) : nullptr;
    }

    bool Codegen::isObject(llvm::Value *value) const {
        auto type = deref(value->getType());
        return type->isStructTy();
    }

    bool Codegen::isClassType(llvm::Type *type) const {
        type = deref(type);
        return type->isStructTy() && mangler.isMangledClass(type->getStructName().str());
    }

    bool Codegen::isInterfaceType(llvm::Type *type) const {
        type = deref(type);
        return type->isStructTy() && mangler.isMangledInterface(type->getStructName().str());
    }

    llvm::StructType *Codegen::genVtable(ClassNode *classNode, llvm::StructType *klass, ClassDecl &classDecl) {
        const auto &classMangledName = mangler.mangleClass(classNode->getName());
        auto &classMethods = classNode->getMethods();
        const auto &virtualMethods = compilerRuntime.virtualMethods.at(classNode->getName());
        std::vector<llvm::Type *> vtableProps;
        vtableProps.reserve(virtualMethods.size());
        uint64_t vtablePos = 0;

        for (auto &methodName: virtualMethods) {
            auto methodDef = classMethods.at(methodName);
            auto fnType = genFnType(methodDef->getFnDef()->getArgs(), methodDef->getFnDef()->getReturnType(), klass);
            vtableProps.push_back(fnType->getPointerTo());
            classDecl.methods[methodName] = {methodDef->getAccessModifier(), true, vtablePos++};
        }

        return llvm::StructType::create(context, vtableProps, "vtable." + classMangledName);
    }

    llvm::StructType *Codegen::genVtable(InterfaceNode *node, llvm::StructType *interfaceType, InterfaceDecl &interfaceDecl) {
        const auto &interfaceMangledName = mangler.mangleInterface(node->getName());
        auto &interfaceMethods = compilerRuntime.interfaceMethods.at(node->getName());
        std::vector<llvm::Type *> vtableProps;
        vtableProps.reserve(interfaceMethods.size());
        uint64_t vtablePos = 0;

        for (auto &[methodName, methodDecl]: interfaceMethods) {
            auto fnType = genFnType(methodDecl->getFnDecl()->getArgs(), methodDecl->getFnDecl()->getReturnType(), interfaceType);
            vtableProps.push_back(fnType->getPointerTo());
            interfaceDecl.methods[methodName] = {methodDecl->getAccessModifier(), true, vtablePos++};
        }

        return llvm::StructType::create(context, vtableProps, "vtable." + interfaceMangledName);
    }

    void Codegen::initVtable(llvm::Value *obj) {
        auto objType = llvm::cast<llvm::StructType>(deref(obj->getType()));
        const auto &mangledClassName = objType->getStructName().str();
        auto currentClassDecl = &getClassDecl(mangledClassName);
        auto &className = currentClassDecl->name;

        while (currentClassDecl) {
            if (currentClassDecl->vtableType) {
                auto vtableName = fmt::format("{}.{}", currentClassDecl->vtableType->getName().str(), className);
                auto vtable = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(vtableName, currentClassDecl->vtableType));

                if (!vtable->hasInitializer()) {
                    std::vector<llvm::Constant *> funcs;

                    for (const auto &it: currentClassDecl->methods) {
                        if (it.second.isVirtual) {
                            auto fn = simpleFindMethod(objType, it.first);
                            // just in case
                            if (!fn) {
                                throw MethodNotFoundException(className, it.first);
                            }
                            funcs.push_back(fn);
                        }
                    }

                    vtable->setInitializer(llvm::ConstantStruct::get(currentClassDecl->vtableType, funcs));
                }

                auto currentObj = builder.CreateBitCast(obj, currentClassDecl->type->getPointerTo());
                auto vtablePos = currentClassDecl->parent ? 1 : 0;
                auto vtablePtr = builder.CreateStructGEP(currentClassDecl->type, currentObj, vtablePos);
                builder.CreateStore(vtable, vtablePtr);
            }

            currentClassDecl = currentClassDecl->parent;
        }
    }

    void Codegen::initInterfaceVtable(llvm::Value *obj, llvm::Value *interface) {
        auto objType = llvm::cast<llvm::StructType>(deref(obj->getType()));
        auto &classDecl = getClassDecl(objType->getName().str());
        auto interfaceType = deref(interface->getType());
        const auto &mangledInterfaceName = interfaceType->getStructName().str();
        auto &interfaceDecl = interfaces.at(mangledInterfaceName);
        auto vtableName = fmt::format("{}.{}", interfaceDecl.vtableType->getName().str(), classDecl.name);
        auto vtable = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(vtableName, interfaceDecl.vtableType));

        if (!vtable->hasInitializer()) {
            std::vector<llvm::Constant *> funcs;
            funcs.reserve(interfaceDecl.methods.size());

            for (const auto &it: interfaceDecl.methods) {
                auto fn = simpleFindMethod(objType, it.first);
                // just in case
                if (!fn) {
                    throw MethodNotFoundException(classDecl.name, it.first);
                }
                funcs.push_back(fn);
            }

            vtable->setInitializer(llvm::ConstantStruct::get(interfaceDecl.vtableType, funcs));
        }

        auto vtablePtr = builder.CreateStructGEP(interfaceType, interface, 0);
        builder.CreateStore(vtable, vtablePtr);
    }

    llvm::Function *Codegen::getInternalConstructor(const std::string &mangledClassName) const {
        return module.getFunction(mangler.mangleInternalMethod(mangledClassName, CONSTRUCTOR_FN_NAME));
    }

    void Codegen::checkConstructor(MethodDefNode *node, const std::string &className) const {
        if (node->getIsStatic()) {
            throw CodegenException(fmt::format("{}::{} cannot be static", className, CONSTRUCTOR_FN_NAME));
        }
        if (node->getAccessModifier() != AccessModifier::PUBLIC) {
            throw CodegenException(fmt::format("{}::{} must be public", className, CONSTRUCTOR_FN_NAME));
        }
        if (node->getFnDef()->getReturnType().getTypeID() != Type::TypeID::VOID) {
            throw CodegenException(fmt::format("{}::{} must return void", className, CONSTRUCTOR_FN_NAME));
        }
    }

    llvm::Value *Codegen::callMethod(llvm::Value *obj, const std::string &methodName, const std::vector<ExprNode *> &args) {
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw CodegenException("invalid method call operand");
        }

        auto [fn, fnType, thisType] = findMethod(llvm::cast<llvm::StructType>(type), methodName, obj);
        if (!fn) {
            throw MethodNotFoundException(getClassName(obj), methodName);
        }

        auto interfaceDecl = findInterfaceDecl(type->getStructName().str());
        if (interfaceDecl) {
            auto objPtr = builder.CreateStructGEP(type, obj, 1);
            obj = builder.CreateLoad(builder.getInt8PtrTy(), objPtr);
            obj = builder.CreateBitCast(obj, thisType->getPointerTo());
        } else if (thisType && thisType != type) {
            obj = builder.CreateBitCast(obj, thisType->getPointerTo());
        }

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size() + 1);
        llvmArgs.push_back(obj);
        for (auto i = 0; i < args.size(); i++) {
            auto val = args[i]->gen(*this);
            val = castTo(val, fnType->getParamType(i + 1)); // skip "this"
            llvmArgs.push_back(val);
        }

        return builder.CreateCall(fnType, fn, llvmArgs);
    }

    llvm::Value *Codegen::callStaticMethod(const std::string &className, const std::string &methodName, const std::vector<ExprNode *> &args) {
        auto &classDecl = className == SELF_KEYWORD && self ? *self : getClassDecl(mangler.mangleClass(className));
        auto [fn, fnType, thisType] = findMethod(classDecl.type, methodName);
        if (!fn) {
            throw MethodNotFoundException(className, methodName);
        }

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size());
        for (auto i = 0; i < args.size(); i++) {
            auto val = args[i]->gen(*this);
            val = castTo(val, fnType->getParamType(i));
            llvmArgs.push_back(val);
        }

        return builder.CreateCall(fnType, fn, llvmArgs);
    }

    std::tuple<llvm::Value *, llvm::FunctionType *, llvm::Type *> Codegen::findMethod(
            llvm::StructType *type, const std::string &methodName, llvm::Value *obj) const {
        if (Runtime::String::isStringType(type) || Runtime::Array::isArrayType(type)) {
            const auto &name = mangler.mangleInternalMethod(type->getName().str(), methodName);
            auto fn = module.getFunction(name);
            return {fn, fn->getFunctionType(), nullptr};
        }

        auto interfaceDecl = findInterfaceDecl(type->getName().str());
        if (interfaceDecl) {
            auto methodIt = interfaceDecl->methods.find(methodName);
            if (methodIt != interfaceDecl->methods.cend()) {
                // get vtable
                auto vtablePtr = builder.CreateStructGEP(type, obj, 0);
                auto vtable = builder.CreateLoad(interfaceDecl->vtableType->getPointerTo(), vtablePtr);
                // get method
                auto methodPtr = builder.CreateStructGEP(interfaceDecl->vtableType, vtable, methodIt->second.vtablePos);
                auto methodType = interfaceDecl->vtableType->getElementType(methodIt->second.vtablePos);
                auto method = builder.CreateLoad(methodType, methodPtr);

                return {method, llvm::cast<llvm::FunctionType>(methodType->getPointerElementType()), type};
            }
        }

        auto &classDecl = getClassDecl(type->getName().str());
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
                    auto currentObj = builder.CreateBitCast(obj, currentClassDecl->type->getPointerTo());
                    auto vtablePos = currentClassDecl->parent ? 1 : 0;
                    auto vtablePtr = builder.CreateStructGEP(currentClassDecl->type, currentObj, vtablePos);
                    auto vtable = builder.CreateLoad(currentClassDecl->vtableType->getPointerTo(), vtablePtr);
                    // get method
                    auto methodPtr = builder.CreateStructGEP(currentClassDecl->vtableType, vtable, methodIt->second.vtablePos);
                    auto methodType = currentClassDecl->vtableType->getElementType(methodIt->second.vtablePos);
                    auto method = builder.CreateLoad(methodType, methodPtr);

                    return {method, llvm::cast<llvm::FunctionType>(methodType->getPointerElementType()), currentClassDecl->type};
                }

                auto fn = module.getFunction(mangler.mangleMethod(currentClassDecl->type->getName().str(), methodName));
                if (fn) {
                    return {fn, fn->getFunctionType(), currentClassDecl->type};
                }
            }

            currentClassDecl = currentClassDecl->parent;
        }

        return {nullptr, nullptr, nullptr};
    }

    llvm::Function *Codegen::simpleFindMethod(llvm::StructType *type, const std::string &methodName) const {
        auto currentClassDecl = &getClassDecl(type->getName().str());
        while (currentClassDecl) {
            const auto &className = currentClassDecl->type->getName().str();
            auto fn = module.getFunction(mangler.mangleMethod(className, methodName));
            if (fn) {
                return fn;
            }

            currentClassDecl = currentClassDecl->parent;
        }

        return nullptr;
    }
}
