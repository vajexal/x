#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    void Codegen::declInterfaces(TopStatementListNode *node) {
        for (auto interfaceNode: node->interfaces) {
            addSymbol(interfaceNode->name);
            const auto &mangledName = mangler->mangleInterface(interfaceNode->name);

            auto interface = llvm::StructType::create(context, mangledName);

            interfaces[interfaceNode->name] = {
                    .name = interfaceNode->name,
                    .type = Type::klass(interfaceNode->name),
                    .llvmType = interface,
            };
        }
    }

    void Codegen::declClasses(TopStatementListNode *node) {
        for (auto klassNode: node->classes) {
            if (classes.contains(klassNode->name)) {
                throw ClassAlreadyExistsException(klassNode->name);
            }

            addSymbol(klassNode->name);

            auto klass = llvm::StructType::create(context, mangler->mangleClass(klassNode->name));

            classes[klassNode->name] = {
                    .name = klassNode->name,
                    .type = Type::klass(klassNode->name),
                    .llvmType = klass,
                    .isAbstract = klassNode->abstract,
            };
        }
    }

    void Codegen::declProps(TopStatementListNode *node) {
        for (auto klassNode: node->classes) {
            const auto &mangledName = mangler->mangleClass(klassNode->name);
            auto &classDecl = classes[klassNode->name];
            auto klass = classDecl.llvmType;
            GC::PointerList pointerList;

            std::vector<llvm::Type *> props;
            props.reserve(klassNode->props.size());
            uint64_t propPos = 0;

            if (klassNode->hasParent()) {
                auto &parentClassDecl = getClassDecl(klassNode->parent);
                props.push_back(parentClassDecl.llvmType);
                propPos++;
                classDecl.parent = const_cast<ClassDecl *>(&parentClassDecl);
                pointerList = parentClassDecl.meta->pointerList;
                classDecl.needInit = parentClassDecl.needInit;
            }

            auto it = compilerRuntime->virtualMethods.find(klassNode->name);
            if (it != compilerRuntime->virtualMethods.cend() && !it->second.empty()) {
                auto vtableType = genVtable(klassNode, classDecl);
                props.push_back(builder.getPtrTy());
                propPos++;
                classDecl.vtableType = vtableType;
            }

            for (auto prop: klassNode->props) {
                auto &propName = prop->decl->name;
                auto &type = prop->decl->type;
                auto llvmType = mapType(type);

                if (prop->isStatic) {
                    // check if static prop already declared here, because module.getOrInsertGlobal could return ConstantExpr
                    // (if prop will be redeclared with different type)
                    if (classDecl.staticProps.contains(propName)) {
                        throw PropAlreadyDeclaredException(klassNode->name, propName);
                    }
                    const auto &mangledPropName = mangler->mangleStaticProp(mangledName, propName);
                    auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, llvmType));
                    classDecl.staticProps[propName] = {global, type, prop->accessModifier};
                } else {
                    props.push_back(llvmType);
                    auto [_, inserted] = classDecl.props.try_emplace(propName, type, propPos++, prop->accessModifier);
                    if (!inserted) {
                        throw PropAlreadyDeclaredException(klassNode->name, propName);
                    }

                    if (prop->decl->expr) {
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
        for (auto klass: node->classes) {
            const auto &mangledName = mangler->mangleClass(klass->name);
            auto classDecl = &classes[klass->name];

            // default constructor
            classDecl->methods[CONSTRUCTOR_FN_NAME] = {
                    AccessModifier::PUBLIC,
                    llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false),
                    false
            };

            for (auto &[methodName, methodDef]: klass->methods) {
                if (methodName == CONSTRUCTOR_FN_NAME) {
                    checkConstructor(methodDef, klass->name);
                }

                auto fnDecl = methodDef->fnDef->decl;
                const auto &fnName = mangler->mangleMethod(mangledName, fnDecl->name);
                auto fnType = genFnType(fnDecl->args, fnDecl->returnType, methodDef->isStatic ? nullptr : &classDecl->type);
                llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, module);
                classDecl->methods[methodName] = {methodDef->accessModifier, fnType, false};
            }
        }
    }

    void Codegen::declFuncs(TopStatementListNode *node) {
        for (auto fnDef: node->funcs) {
            auto fnDecl = fnDef->decl;

            if (module.getFunction(fnDecl->name)) {
                throw FnAlreadyDeclaredException(fnDecl->name);
            }

            if (fnDecl->name == MAIN_FN_NAME) {
                checkMainFn(fnDef);
            }

            auto fnType = genFnType(fnDecl->args, fnDecl->returnType);
            llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnDecl->name, module);
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
        for (auto decl: node->globals) {
            genGlobal(decl);
        }

        // set initializers for static props
        for (auto klass: node->classes) {
            for (auto prop: klass->props) {
                if (prop->isStatic) {
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

        auto &name = node->name;
        if (vars.contains(name)) {
            throw VarAlreadyExistsException(name);
        }

        auto &type = node->type;
        auto llvmType = mapType(type);
        auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(name, llvmType));

        auto value = node->expr ?
                     castTo(node->expr->gen(*this), node->expr->type, type) :
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
        auto decl = prop->decl;
        auto &type = decl->type;
        auto llvmType = mapType(decl->type);
        auto mangledClassName = mangler->mangleClass(klass->name);
        auto mangledPropName = mangler->mangleStaticProp(mangledClassName, decl->name);
        auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, llvmType));

        auto value = decl->expr ?
                     castTo(decl->expr->gen(*this), decl->expr->type, type) :
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

        for (auto prop: node->props) {
            if (prop->isStatic || !prop->decl->expr) {
                continue;
            }

            auto decl = prop->decl;
            auto &type = decl->type;
            auto value = castTo(decl->expr->gen(*this), decl->expr->type, type);
            auto ptr = builder.CreateStructGEP(classDecl.llvmType, initFnThis, classDecl.props.at(decl->name).pos);

            builder.CreateStore(value, ptr);
        }

        builder.CreateRetVoid();
    }

    void Codegen::checkConstructor(MethodDefNode *node, const std::string &className) const {
        if (node->isStatic) {
            throw CodegenException(fmt::format("{}::{} cannot be static", className, CONSTRUCTOR_FN_NAME));
        }
        if (node->accessModifier != AccessModifier::PUBLIC) {
            throw CodegenException(fmt::format("{}::{} must be public", className, CONSTRUCTOR_FN_NAME));
        }
        if (!node->fnDef->decl->returnType.is(Type::TypeID::VOID)) {
            throw CodegenException(fmt::format("{}::{} must return void", className, CONSTRUCTOR_FN_NAME));
        }
    }
}
