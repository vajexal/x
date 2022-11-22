#include <optional>
#include <fmt/core.h>

#include "codegen.h"
#include "runtime/runtime.h"
#include "utils.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(ClassNode *node) {
        auto &name = node->getName();
        const auto &mangledName = mangler.mangleClass(name);
        std::vector<llvm::Type *> props;
        props.reserve(node->getProps().size());
        ClassDecl classDecl;
        uint64_t propIndex = 0;

        classDecl.isAbstract = node->isAbstract();

        if (node->hasParent()) {
            const auto &mangledParentName = mangler.mangleClass(node->getParent());
            auto parentClassDecl = classes.find(mangledParentName);
            if (parentClassDecl == classes.end()) {
                throw CodegenException(fmt::format("class {} not found", node->getParent()));
            }

            props.push_back(parentClassDecl->second.type);
            propIndex++;
            classDecl.parent = &parentClassDecl->second;
        }

        for (auto prop: node->getProps()) {
            if (prop->getType().getTypeID() == Type::TypeID::VOID) {
                throw InvalidTypeException();
            }

            auto &propName = prop->getName();
            auto type = mapType(prop->getType());
            if (prop->getIsStatic()) {
                const auto &mangledPropName = mangler.mangleStaticProp(mangledName, propName);
                auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, type));
                global->setInitializer(getDefaultValue(prop->getType()));
                classDecl.staticProps[propName] = {global, prop->getAccessModifier()};
            } else {
                props.push_back(type);
                classDecl.props[propName] = {type, propIndex++, prop->getAccessModifier()};
            }
        }

        auto klass = llvm::StructType::create(context, props, mangledName);
        classDecl.type = klass;
        classes[mangledName] = std::move(classDecl);
        self = &classes[mangledName];

        for (auto method: node->getMethods()) {
            auto fn = method->getFnDef();
            auto isConstructor = fn->getName() == CONSTRUCTOR_FN_NAME;
            if (isConstructor) {
                checkConstructor(method, name);
            }
            const auto &fnName = mangler.mangleMethod(mangledName, fn->getName());
            std::optional<Type> thisType = method->getIsStatic() ? std::nullopt : std::optional<Type>(Type::klass(name));
            genFn(fnName, fn->getArgs(), fn->getReturnType(), fn->getBody(), thisType);
            if (!isConstructor) {
                self->methods[fn->getName()] = {method->getAccessModifier()};
            }
        }

        self->methods[CONSTRUCTOR_FN_NAME] = {AccessModifier::PUBLIC};

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
            throw MethodNotFoundException(methodName);
        }

        return callMethod(obj, methodName, node->getArgs());
    }

    llvm::Value *Codegen::gen(StaticMethodCallNode *node) {
        auto &methodName = node->getMethodName();
        const auto &className = mangler.mangleClass(node->getClassName());
        auto &classDecl = getClass(className);
        auto [callee, thisType] = findMethod(classDecl.type, methodName);
        if (!callee) {
            throw CodegenException("method not found: " + methodName);
        }

        if (callee->arg_size() != node->getArgs().size()) {
            throw CodegenException("callee args mismatch");
        }

        std::vector<llvm::Value *> args;
        args.reserve(node->getArgs().size());
        for (auto arg: node->getArgs()) {
            args.push_back(arg->gen(*this));
        }

        return builder.CreateCall(callee, args);
    }

    llvm::Value *Codegen::gen(AssignPropNode *node) {
        auto obj = node->getObj()->gen(*this);
        if (!isObject(obj)) {
            throw InvalidObjectAccessException();
        }

        auto value = node->getExpr()->gen(*this);
        auto [type, ptr] = getProp(obj, node->getName());
        builder.CreateStore(value, ptr);
        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignStaticPropNode *node) {
        auto value = node->getExpr()->gen(*this);
        auto [type, ptr] = getStaticProp(node->getClassName(), node->getPropName());
        builder.CreateStore(value, ptr);
        return nullptr;
    }

    llvm::Value *Codegen::gen(NewNode *node) {
        const auto &mangledClassName = mangler.mangleClass(node->getName());
        auto &classDecl = getClass(mangledClassName);
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

        return obj;
    }

    llvm::Value *Codegen::gen(InterfaceNode *node) {
        return nullptr;
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getProp(llvm::Value *obj, const std::string &name) const {
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw InvalidObjectAccessException();
        }

        auto className = type->getStructName();
        auto &classDecl = getClass(className.str());
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto prop = currentClassDecl->props.find(name);
            if (prop != currentClassDecl->props.end()) {
                if (!that && prop->second.accessModifier != AccessModifier::PUBLIC) {
                    throw CodegenException("cannot access private property: " + name);
                }

                auto currentObj = currentClassDecl == &classDecl ? obj : builder.CreateBitCast(obj, currentClassDecl->type->getPointerTo());
                auto ptr = builder.CreateStructGEP(currentClassDecl->type, currentObj, prop->second.pos);
                return {prop->second.type, ptr};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        throw CodegenException("prop not found: " + name);
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getStaticProp(const std::string &className, const std::string &propName) const {
        const auto &mangledClassName = mangler.mangleClass(className);
        auto &classDecl = getClass(mangledClassName);
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto prop = currentClassDecl->staticProps.find(propName);
            if (prop != currentClassDecl->staticProps.end()) {
                if (!self && prop->second.accessModifier != AccessModifier::PUBLIC) {
                    throw CodegenException("cannot access private property: " + propName);
                }

                return {prop->second.var->getType()->getPointerElementType(), prop->second.var};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        throw CodegenException("prop not found: " + propName);
    }

    const ClassDecl &Codegen::getClass(const std::string &mangledName) const {
        auto classDecl = classes.find(mangledName);
        if (classDecl == classes.end()) {
            throw CodegenException("class not found: " + mangler.unmangleClass(mangledName));
        }
        return classDecl->second;
    }

    bool Codegen::isObject(llvm::Value *value) const {
        auto type = deref(value->getType());
        return type->isStructTy();
    }

    AccessModifier Codegen::getMethodAccessModifier(const std::string &mangledClassName, const std::string &methodName) const {
        auto &classDecl = getClass(mangledClassName);
        auto method = classDecl.methods.find(methodName);
        if (method == classDecl.methods.end()) {
            throw CodegenException("method not found: " + methodName);
        }
        return method->second.accessModifier;
    }

    llvm::Function *Codegen::getConstructor(const std::string &mangledClassName) const {
        return module.getFunction(mangler.mangleMethod(mangledClassName, CONSTRUCTOR_FN_NAME));
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

        auto [callee, thisType] = findMethod(llvm::cast<llvm::StructType>(type), methodName);
        if (!callee) {
            throw MethodNotFoundException(methodName);
        }

        if (callee->arg_size() != (args.size() + 1)) {
            throw CodegenException("callee args mismatch");
        }

        if (thisType) {
            obj = builder.CreateBitCast(obj, thisType->getPointerTo());
        }

        std::vector<llvm::Value *> llvmArgs;
        llvmArgs.reserve(args.size() + 1);
        llvmArgs.push_back(obj);
        for (auto arg: args) {
            llvmArgs.push_back(arg->gen(*this));
        }

        return builder.CreateCall(callee, llvmArgs);
    }

    std::pair<llvm::Function *, llvm::Type *> Codegen::findMethod(llvm::StructType *type, const std::string &methodName) const {
        if (Runtime::String::isStringType(type) || Runtime::Array::isArrayType(type)) {
            const auto &name = mangler.mangleMethod(type->getName().str(), methodName);
            return {module.getFunction(name), nullptr};
        }

        auto &classDecl = getClass(type->getName().str());
        auto currentClassDecl = &classDecl;
        while (currentClassDecl) {
            const auto &className = currentClassDecl->type->getName().str();
            auto callee = module.getFunction(mangler.mangleMethod(className, methodName));
            if (callee) {
                auto methodAccessModifier = getMethodAccessModifier(className, methodName);
                if (!that && methodAccessModifier != AccessModifier::PUBLIC) {
                    throw CodegenException("cannot access private method: " + methodName);
                }
                return {callee, currentClassDecl != &classDecl ? currentClassDecl->type : nullptr};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        return {nullptr, nullptr};
    }
}
