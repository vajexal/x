#include <optional>
#include <fmt/core.h>

#include "codegen.h"

namespace X::Codegen {
    llvm::Value *Codegen::gen(ClassNode *node) {
        auto name = node->getName();
        auto mangledName = mangler.mangleClass(name);
        auto members = node->getMembers();
        std::vector<llvm::Type *> props;
        props.reserve(members->getProps().size());
        ClassDecl classDecl;
        uint64_t propIndex = 0;

        classDecl.isAbstract = node->isAbstract();

        if (node->hasParent()) {
            auto mangledParentName = mangler.mangleClass(node->getParent());
            auto parentClassDecl = classes.find(mangledParentName);
            if (parentClassDecl == classes.end()) {
                throw CodegenException(fmt::format("class {} not found", node->getParent()));
            }

            props.push_back(parentClassDecl->second.type);
            propIndex++;
            classDecl.parent = &parentClassDecl->second;
        }

        for (auto prop: members->getProps()) {
            auto propName = prop->getName();
            auto type = mapType(prop->getType());
            if (prop->getIsStatic()) {
                auto mangledPropName = mangler.mangleStaticProp(mangledName, propName);
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

        for (auto method: members->getMethods()) {
            auto fn = method->getFnDef();
            auto fnName = mangler.mangleMethod(mangledName, fn->getName());
            std::optional<Type> thisType = method->getIsStatic() ? std::nullopt : std::optional<Type>(std::move(Type(name)));
            genFn(fnName, fn->getArgs(), fn->getReturnType(), fn->getBody(), thisType);
            classes[mangledName].methods[fn->getName()] = {method->getAccessModifier()};
        }

        self = nullptr;

        return nullptr;
    }

    llvm::Value *Codegen::gen(FetchPropNode *node) {
        auto obj = node->getObj()->gen(*this);
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
        auto &methodName = node->getName();
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw CodegenException("invalid method call operand");
        }

        auto [callee, thisType] = findMethod(llvm::cast<llvm::StructType>(type), methodName);
        if (!callee) {
            throw CodegenException("method is not found: " + methodName);
        }

        if (callee->arg_size() != (node->getArgs().size() + 1)) {
            throw CodegenException("callee args mismatch");
        }

        if (thisType) {
            obj = builder.CreateBitCast(obj, thisType->getPointerTo());
        }

        std::vector<llvm::Value *> args;
        args.reserve(node->getArgs().size() + 1);
        args.push_back(obj);
        for (auto arg: node->getArgs()) {
            args.push_back(arg->gen(*this));
        }

        return builder.CreateCall(callee, args);
    }

    llvm::Value *Codegen::gen(StaticMethodCallNode *node) {
        const auto &methodName = node->getMethodName();
        auto className = mangler.mangleClass(node->getClassName());
        auto classDecl = getClass(className);
        auto [callee, thisType] = findMethod(classDecl.type, methodName);
        if (!callee) {
            throw CodegenException("method is not found: " + methodName);
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
        auto classDecl = getClass(mangler.mangleClass(node->getName()));
        if (classDecl.isAbstract) {
            throw CodegenException("cannot instantiate abstract class " + node->getName());
        }

        return builder.CreateAlloca(classDecl.type);
    }

    llvm::Value *Codegen::gen(InterfaceNode *node) {
        return nullptr;
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getProp(llvm::Value *obj, const std::string &name) const {
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw CodegenException("invalid obj operand");
        }

        auto className = type->getStructName();
        auto classDecl = getClass(className.str());
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
        auto mangledClassName = mangler.mangleClass(className);
        auto classDecl = getClass(mangledClassName);
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

    AccessModifier Codegen::getMethodAccessModifier(const std::string &mangledClassName, const std::string &methodName) const {
        auto classDecl = getClass(mangledClassName);
        auto method = classDecl.methods.find(methodName);
        if (method == classDecl.methods.end()) {
            throw CodegenException("method not found: " + methodName);
        }
        return method->second.accessModifier;
    }

    llvm::Function *Codegen::getConstructor(const std::string &mangledClassName) const {
        return module.getFunction(mangler.mangleMethod(mangledClassName, "construct"));
    }

    std::pair<llvm::Function *, llvm::Type *> Codegen::findMethod(llvm::StructType *type, const std::string &methodName) const {
        if (isStringType(type)) {
            auto name = mangler.mangleMethod(type->getName().str(), methodName);
            return {module.getFunction(name), nullptr};
        }

        auto classDecl = getClass(type->getName().str());
        const ClassDecl *currentClassDecl = &classDecl;
        while (currentClassDecl) {
            auto className = currentClassDecl->type->getName().str();
            auto name = mangler.mangleMethod(className, methodName);
            auto callee = module.getFunction(name);
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
