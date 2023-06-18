#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    void Codegen::declInterfaces(TopStatementListNode *node) {
        for (auto interfaceNode: node->getInterfaces()) {
            auto &name = interfaceNode->getName();
            addSymbol(name);
            const auto &mangledName = mangler.mangleInterface(name);

            auto interface = llvm::StructType::create(context, mangledName);

            InterfaceDecl interfaceDecl;
            interfaceDecl.name = name;
            interfaceDecl.type = interface;
            interfaces[mangledName] = std::move(interfaceDecl);
        }
    }

    void Codegen::declClasses(TopStatementListNode *node) {
        for (auto klassNode: node->getClasses()) {
            auto &name = klassNode->getName();
            addSymbol(name);
            const auto &mangledName = mangler.mangleClass(name);
            if (classes.contains(mangledName)) {
                throw ClassAlreadyExistsException(name);
            }

            auto klass = llvm::StructType::create(context, mangledName);

            classes[mangledName] = {
                    .name = name,
                    .type = klass,
                    .isAbstract = klassNode->isAbstract(),
            };
        }
    }

    void Codegen::declProps(TopStatementListNode *node) {
        for (auto klassNode: node->getClasses()) {
            auto &name = klassNode->getName();
            const auto &mangledName = mangler.mangleClass(name);
            auto &classDecl = classes[mangledName];
            auto klass = classDecl.type;
            GC::PointerList pointerList;

            std::vector<llvm::Type *> props;
            props.reserve(klassNode->getProps().size());
            uint64_t propPos = 0;

            if (klassNode->hasParent()) {
                const auto &mangledParentName = mangler.mangleClass(klassNode->getParent());
                auto &parentClassDecl = getClassDecl(mangledParentName);
                props.push_back(parentClassDecl.type);
                propPos++;
                classDecl.parent = const_cast<ClassDecl *>(&parentClassDecl);
                pointerList = parentClassDecl.meta->pointerList;
            }

            auto it = compilerRuntime.virtualMethods.find(name);
            if (it != compilerRuntime.virtualMethods.cend() && !it->second.empty()) {
                auto vtableType = genVtable(klassNode, klass, classDecl);
                props.push_back(vtableType->getPointerTo());
                propPos++;
                classDecl.vtableType = vtableType;
            }

            for (auto prop: klassNode->getProps()) {
                auto &propName = prop->getName();
                auto type = mapType(prop->getType());
                if (prop->getIsStatic()) {
                    // check if static prop already declared here, because module.getOrInsertGlobal could return ConstantExpr
                    // (if prop will be redeclared with different type)
                    if (classDecl.staticProps.contains(propName)) {
                        throw PropAlreadyDeclaredException(name, propName);
                    }
                    const auto &mangledPropName = mangler.mangleStaticProp(mangledName, propName);
                    auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, type));
                    global->setInitializer(getDefaultValue(prop->getType()));
                    classDecl.staticProps[propName] = {global, prop->getAccessModifier()};
                } else {
                    props.push_back(type);
                    auto [_, inserted] = classDecl.props.try_emplace(propName, type, propPos++, prop->getAccessModifier());
                    if (!inserted) {
                        throw PropAlreadyDeclaredException(name, propName);
                    }
                }
            }

            klass->setBody(props);

            // build class pointer list
            auto structLayout = module.getDataLayout().getStructLayout(klass);
            for (auto i = 0; i < klass->getNumElements(); i++) {
                auto type = klass->getElementType(i);
                if (type->isPointerTy()) {
                    auto offset = structLayout->getElementOffset(i);
                    auto meta = getTypeGCMeta(type);
                    pointerList.emplace_back(offset, meta);
                }
            }

            classDecl.meta = gc.addMeta(GC::NodeType::CLASS, std::move(pointerList));
        }
    }

    void Codegen::declMethods(TopStatementListNode *node) {
        for (auto klass: node->getClasses()) {
            const auto &mangledName = mangler.mangleClass(klass->getName());
            auto classDecl = &classes[mangledName];

            for (auto &[methodName, methodDef]: klass->getMethods()) {
                if (methodName == CONSTRUCTOR_FN_NAME) {
                    continue;
                }

                auto fnDef = methodDef->getFnDef();
                const auto &fnName = mangler.mangleMethod(mangledName, fnDef->getName());
                auto fnType = genFnType(fnDef->getArgs(), fnDef->getReturnType(), methodDef->getIsStatic() ? nullptr : classDecl->type);
                llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, module);
                classDecl->methods[methodName] = {methodDef->getAccessModifier(), false};
            }
        }
    }

    void Codegen::declFuncs(TopStatementListNode *node) {
        for (auto fnDef: node->getFuncs()) {
            auto &name = fnDef->getName();

            if (module.getFunction(name)) {
                throw FnAlreadyDeclaredException(name);
            }

            if (name == MAIN_FN_NAME) {
                checkMainFn(fnDef);
            }

            auto fnType = genFnType(fnDef->getArgs(), fnDef->getReturnType());
            llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, name, module);
        }
    }
}
