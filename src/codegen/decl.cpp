#include "codegen.h"

#include "utils.h"

namespace X::Codegen {
    void Codegen::declInterfaces(TopStatementListNode *node) {
        for (auto interfaceNode: node->getInterfaces()) {
            auto &name = interfaceNode->getName();
            addSymbol(name);
            const auto &mangledName = Mangler::mangleInterface(name);

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

            auto klass = llvm::StructType::create(context, Mangler::mangleClass(name));

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
            const auto &mangledName = Mangler::mangleClass(name);
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
            }

            auto it = compilerRuntime.virtualMethods.find(name);
            if (it != compilerRuntime.virtualMethods.cend() && !it->second.empty()) {
                auto vtableType = genVtable(klassNode, classDecl);
                props.push_back(builder.getPtrTy());
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
                    const auto &mangledPropName = Mangler::mangleStaticProp(mangledName, propName);
                    auto global = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal(mangledPropName, type));
                    global->setInitializer(getDefaultValue(prop->getType()));
                    classDecl.staticProps[propName] = {global, prop->getType(), prop->getAccessModifier()};
                } else {
                    props.push_back(type);
                    auto [_, inserted] = classDecl.props.try_emplace(propName, prop->getType(), propPos++, prop->getAccessModifier());
                    if (!inserted) {
                        throw PropAlreadyDeclaredException(name, propName);
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

            classDecl.meta = gc.addMeta(GC::NodeType::CLASS, std::move(pointerList));
        }
    }

    void Codegen::declMethods(TopStatementListNode *node) {
        for (auto klass: node->getClasses()) {
            const auto &mangledName = Mangler::mangleClass(klass->getName());
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

                auto fnDef = methodDef->getFnDef();
                const auto &fnName = Mangler::mangleMethod(mangledName, fnDef->getName());
                auto fnType = genFnType(fnDef->getArgs(), fnDef->getReturnType(), methodDef->getIsStatic() ? nullptr : &classDecl->type);
                llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, module);
                classDecl->methods[methodName] = {methodDef->getAccessModifier(), fnType, false};
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

    void Codegen::checkConstructor(MethodDefNode *node, const std::string &className) const {
        if (node->getIsStatic()) {
            throw CodegenException(fmt::format("{}::{} cannot be static", className, CONSTRUCTOR_FN_NAME));
        }
        if (node->getAccessModifier() != AccessModifier::PUBLIC) {
            throw CodegenException(fmt::format("{}::{} must be public", className, CONSTRUCTOR_FN_NAME));
        }
        if (!node->getFnDef()->getReturnType().is(Type::TypeID::VOID)) {
            throw CodegenException(fmt::format("{}::{} must return void", className, CONSTRUCTOR_FN_NAME));
        }
    }
}
