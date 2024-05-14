#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    void Codegen::declInterfaces(TopStatementListNode *node) {
        for (auto interfaceNode: node->getInterfaces()) {
            auto &name = interfaceNode->getName();
            addSymbol(name);
            const auto &mangledName = mangler->mangleInterface(name);

            auto interface = llvm::StructType::create(context, mangledName);

            interfaces[name] = {
                    .name = name,
                    .type = Type::klass(name),
                    .llvmType = interface,
            };
        }
    }

    void Codegen::declClasses(TopStatementListNode *node) {
        for (auto klassNode: node->getClasses()) {
            auto &name = klassNode->getName();
            if (classes.contains(name)) {
                throw ClassAlreadyExistsException(name);
            }

            addSymbol(name);

            auto klass = llvm::StructType::create(context, mangler->mangleClass(name));

            classes[name] = {
                    .name = name,
                    .type = Type::klass(name),
                    .llvmType = klass,
                    .isAbstract = klassNode->isAbstract(),
            };
        }
    }

    void Codegen::declProps(TopStatementListNode *node) {
        for (auto klassNode: node->getClasses()) {
            auto &name = klassNode->getName();
            const auto &mangledName = mangler->mangleClass(name);
            auto &classDecl = classes[name];
            auto klass = classDecl.llvmType;
            GC::PointerList pointerList;

            std::vector<llvm::Type *> props;
            props.reserve(klassNode->getProps().size());
            uint64_t propPos = 0;

            if (klassNode->hasParent()) {
                auto &parentClassDecl = getClassDecl(klassNode->getParent());
                props.push_back(parentClassDecl.llvmType);
                propPos++;
                classDecl.parent = const_cast<ClassDecl *>(&parentClassDecl);
                pointerList = parentClassDecl.meta->pointerList;
                classDecl.needInit = parentClassDecl.needInit;
            }

            auto it = compilerRuntime->virtualMethods.find(name);
            if (it != compilerRuntime->virtualMethods.cend() && !it->second.empty()) {
                auto vtableType = genVtable(klassNode, classDecl);
                props.push_back(builder.getPtrTy());
                propPos++;
                classDecl.vtableType = vtableType;
            }

            for (auto prop: klassNode->getProps()) {
                auto &propName = prop->getDecl()->getName();
                auto &type = prop->getDecl()->getType();
                auto llvmType = mapType(type);

                if (prop->getIsStatic()) {
                    // check if static prop already declared here, because module.getOrInsertGlobal could return ConstantExpr
                    // (if prop will be redeclared with different type)
                    if (classDecl.staticProps.contains(propName)) {
                        throw PropAlreadyDeclaredException(name, propName);
                    }
                    const auto &mangledPropName = mangler->mangleStaticProp(mangledName, propName);
                    auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, llvmType));
                    classDecl.staticProps[propName] = {global, type, prop->getAccessModifier()};
                } else {
                    props.push_back(llvmType);
                    auto [_, inserted] = classDecl.props.try_emplace(propName, type, propPos++, prop->getAccessModifier());
                    if (!inserted) {
                        throw PropAlreadyDeclaredException(name, propName);
                    }

                    if (prop->getDecl()->getExpr()) {
                        classDecl.needInit = true;
                    }
                }
            }

            klass->setBody(props);

            // build class pointer list
            auto structLayout = module.getDataLayout().getStructLayout(klass);
            for (auto &[_, prop]: classDecl.props) {
                if (prop.type.isOneOf(Type::TypeID::CLASS, Type::TypeID::STRING, Type::TypeID::ARRAY)) {
                    const auto &offset = structLayout->getElementOffset(prop.pos);
                    auto meta = getTypeGCMeta(prop.type);
                    pointerList.emplace_back(offset, meta);
                }
            }

            classDecl.meta = gc->addMeta(GC::NodeType::CLASS, std::move(pointerList));

            if (classDecl.needInit) {
                genClassInit(klassNode, classDecl);
            }
        }
    }

    void Codegen::declMethods(TopStatementListNode *node) {
        for (auto klass: node->getClasses()) {
            const auto &mangledName = mangler->mangleClass(klass->getName());
            auto classDecl = &classes[klass->getName()];

            // default constructor
            classDecl->methods[CONSTRUCTOR_FN_NAME] = {
                    AccessModifier::PUBLIC,
                    llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false),
                    false
            };

            for (auto &[methodName, methodDef]: klass->getMethods()) {
                if (methodName == CONSTRUCTOR_FN_NAME) {
                    checkConstructor(methodDef, klass->getName());
                }

                auto fnDecl = methodDef->getFnDef()->getDecl();
                const auto &fnName = mangler->mangleMethod(mangledName, fnDecl->getName());
                auto fnType = genFnType(fnDecl->getArgs(), fnDecl->getReturnType(), methodDef->getIsStatic() ? nullptr : &classDecl->type);
                llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, module);
                classDecl->methods[methodName] = {methodDef->getAccessModifier(), fnType, false};
            }
        }
    }

    void Codegen::declFuncs(TopStatementListNode *node) {
        for (auto fnDef: node->getFuncs()) {
            auto fnDecl = fnDef->getDecl();
            auto &name = fnDecl->getName();

            if (module.getFunction(name)) {
                throw FnAlreadyDeclaredException(name);
            }

            if (name == MAIN_FN_NAME) {
                checkMainFn(fnDef);
            }

            auto fnType = genFnType(fnDecl->getArgs(), fnDecl->getReturnType());
            llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, name, module);
        }
    }

    void Codegen::declGlobals(TopStatementListNode *node) {
        // create init function
        auto initFn = llvm::Function::Create(
                llvm::FunctionType::get(builder.getVoidTy(), {}, false),
                llvm::Function::ExternalLinkage,
                mangler->mangleInternalFunction(INIT_FN_NAME),
                module
        );
        auto bb = llvm::BasicBlock::Create(context, "entry", initFn);
        builder.SetInsertPoint(bb);

        varScopes.emplace_back();
        auto &vars = varScopes.back();

        // declare global variables
        for (auto decl: node->getGlobals()) {
            genGlobal(decl);
        }

        // set initializers for static props
        for (auto klass: node->getClasses()) {
            for (auto prop: klass->getProps()) {
                if (prop->getIsStatic()) {
                    genStaticPropInit(prop, klass);
                }
            }
        }

        if (bb->empty()) {
            initFn->eraseFromParent();

            return;
        }

        builder.CreateRetVoid();
    }

    void Codegen::genGlobal(DeclNode *node) {
        auto &vars = varScopes.back();

        auto &name = node->getName();
        if (vars.contains(name)) {
            throw VarAlreadyExistsException(name);
        }

        auto &type = node->getType();
        auto llvmType = mapType(type);
        auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(name, llvmType));

        auto value = node->getExpr() ?
                     castTo(node->getExpr()->gen(*this), node->getExpr()->getType(), type) :
                     createDefaultValue(type);

        if (auto constValue = llvm::dyn_cast<llvm::Constant>(value)) {
            global->setInitializer(constValue);
        } else {
            global->setInitializer(getDefaultValue(type));
            builder.CreateStore(value, global);
        }

        vars[name] = {global, type};

        gcAddGlobalRoot(global, type);
    }

    void Codegen::genStaticPropInit(PropDeclNode *prop, ClassNode *klass) {
        auto decl = prop->getDecl();
        auto &type = decl->getType();
        auto llvmType = mapType(decl->getType());
        auto mangledClassName = mangler->mangleClass(klass->getName());
        auto mangledPropName = mangler->mangleStaticProp(mangledClassName, decl->getName());
        auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, llvmType));

        auto value = decl->getExpr() ?
                     castTo(decl->getExpr()->gen(*this), decl->getExpr()->getType(), type) :
                     createDefaultValue(type);

        if (auto constValue = llvm::dyn_cast<llvm::Constant>(value)) {
            global->setInitializer(constValue);
        } else {
            global->setInitializer(getDefaultValue(type));
            builder.CreateStore(value, global);
        }

        gcAddGlobalRoot(global, type);
    }

    void Codegen::genClassInit(ClassNode *node, const ClassDecl &classDecl) {
        auto mangledName = mangler->mangleClass(classDecl.name);

        // create init function
        auto initFn = llvm::Function::Create(
                llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false),
                llvm::Function::ExternalLinkage,
                mangler->mangleHiddenMethod(mangledName, INIT_FN_NAME),
                module
        );
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", initFn));

        auto initFnThis = initFn->getArg(0);

        if (classDecl.parent) {
            if (auto parentInitFn = module.getFunction(mangler->mangleHiddenMethod(mangler->mangleClass(classDecl.parent->name), INIT_FN_NAME))) {
                builder.CreateCall(parentInitFn, {initFnThis});
            }
        }

        for (auto prop: node->getProps()) {
            if (prop->getIsStatic() || !prop->getDecl()->getExpr()) {
                continue;
            }

            auto decl = prop->getDecl();
            auto &type = decl->getType();
            auto value = castTo(decl->getExpr()->gen(*this), decl->getExpr()->getType(), type);
            auto ptr = builder.CreateStructGEP(classDecl.llvmType, initFnThis, classDecl.props.at(decl->getName()).pos);

            builder.CreateStore(value, ptr);
        }

        builder.CreateRetVoid();
    }

    void Codegen::checkConstructor(MethodDefNode *node, const std::string &className) const {
        if (node->getIsStatic()) {
            throw CodegenException(fmt::format("{}::{} cannot be static", className, CONSTRUCTOR_FN_NAME));
        }
        if (node->getAccessModifier() != AccessModifier::PUBLIC) {
            throw CodegenException(fmt::format("{}::{} must be public", className, CONSTRUCTOR_FN_NAME));
        }
        if (!node->getFnDef()->getDecl()->getReturnType().is(Type::TypeID::VOID)) {
            throw CodegenException(fmt::format("{}::{} must return void", className, CONSTRUCTOR_FN_NAME));
        }
    }
}
