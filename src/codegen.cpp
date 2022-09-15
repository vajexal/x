#include <optional>
#include <fmt/core.h>

#include "llvm/IR/GlobalValue.h"

#include "codegen.h"

namespace X {
    llvm::Value *Codegen::gen(Node *node) {
        throw CodegenException("can't gen node");
    }

    llvm::Value *Codegen::gen(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            child->gen(*this);
        }

        return nullptr;
    }

    llvm::Value *Codegen::gen(ScalarNode *node) {
        auto value = node->getValue();

        switch (node->getType().getTypeID()) {
            case Type::TypeID::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), std::get<int>(value));
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getFloatTy(context), std::get<float>(value));
            case Type::TypeID::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), std::get<bool>(value));
            default:
                throw CodegenException("invalid scalar type");
        }
    }

    llvm::Value *Codegen::gen(UnaryNode *node) {
        auto opType = node->getType();
        auto expr = node->getExpr()->gen(*this);

        switch (opType) {
            case OpType::PRE_INC:
            case OpType::PRE_DEC:
            case OpType::POST_INC:
            case OpType::POST_DEC: {
                llvm::Value *value;
                switch (expr->getType()->getTypeID()) {
                    case llvm::Type::TypeID::IntegerTyID:
                        value = opType == OpType::PRE_INC || opType == OpType::POST_INC ?
                                builder.CreateAdd(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1)) :
                                builder.CreateSub(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
                        break;
                    case llvm::Type::TypeID::FloatTyID:
                        value = opType == OpType::PRE_INC || opType == OpType::POST_INC ?
                                builder.CreateFAdd(expr, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 1)) :
                                builder.CreateFSub(expr, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 1));
                        break;
                    default:
                        throw CodegenException("invalid type");
                }

                auto name = dynamic_cast<VarNode *>(node->getExpr())->getName(); // todo
                auto [type, var] = getVar(name);
                builder.CreateStore(value, var);

                return opType == OpType::PRE_INC || opType == OpType::PRE_DEC ? value : expr;
            }
            case OpType::NOT:
                expr = downcastToBool(expr);
                return builder.CreateXor(expr, llvm::ConstantInt::getTrue(context));
            default:
                throw CodegenException("invalid op type");
        }
    }

    llvm::Value *Codegen::gen(BinaryNode *node) {
        auto lhs = node->getLhs()->gen(*this);
        auto rhs = node->getRhs()->gen(*this);

        if (!lhs || !rhs) {
            throw CodegenException("binary arg is empty");
        }

        if (node->getType() == OpType::DIV) {
            std::tie(lhs, rhs) = forceUpcast(lhs, rhs);
        } else {
            std::tie(lhs, rhs) = upcast(lhs, rhs);
        }

        using binaryExprGenerator = std::function<llvm::Value *(llvm::Value *, llvm::Value *)>;

        // todo maybe refactor to switches
        static std::map<llvm::Type::TypeID, std::map<OpType, binaryExprGenerator>> binaryExprGeneratorMap{
            {llvm::Type::TypeID::IntegerTyID, {
                {OpType::PLUS,             [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateAdd(lhs, rhs); }},
                {OpType::MINUS,            [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateSub(lhs, rhs); }},
                {OpType::MUL,              [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateMul(lhs, rhs); }},
                {OpType::EQUAL,            [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpEQ(lhs, rhs); }},
                {OpType::NOT_EQUAL,        [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpNE(lhs, rhs); }},
                {OpType::SMALLER,          [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSLT(lhs, rhs); }},
                {OpType::SMALLER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSLE(lhs, rhs); }},
                {OpType::GREATER,          [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSGT(lhs, rhs); }},
                {OpType::GREATER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateICmpSGE(lhs, rhs); }}
            }},
            {llvm::Type::TypeID::FloatTyID, {
                {OpType::PLUS,             [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFAdd(lhs, rhs); }},
                {OpType::MINUS,            [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFSub(lhs, rhs); }},
                {OpType::MUL,              [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFMul(lhs, rhs); }},
                {OpType::DIV,              [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFDiv(lhs, rhs); }},
                {OpType::EQUAL,            [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOEQ(lhs, rhs); }},
                {OpType::NOT_EQUAL,        [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpONE(lhs, rhs); }},
                {OpType::SMALLER,          [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOLT(lhs, rhs); }},
                {OpType::SMALLER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOLE(lhs, rhs); }},
                {OpType::GREATER,          [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOGT(lhs, rhs); }},
                {OpType::GREATER_OR_EQUAL, [this](llvm::Value *lhs, llvm::Value *rhs) { return builder.CreateFCmpOGE(lhs, rhs); }}
            }}
        };

        auto typeGeneratorMap = binaryExprGeneratorMap.find(lhs->getType()->getTypeID());
        if (typeGeneratorMap == binaryExprGeneratorMap.end()) {
            throw CodegenException("invalid type");
        }

        auto exprGenerator = typeGeneratorMap->second.find(node->getType());
        if (exprGenerator == typeGeneratorMap->second.end()) {
            throw CodegenException("invalid op type");
        }

        return exprGenerator->second(lhs, rhs);
    }

    llvm::Value *Codegen::gen(DeclareNode *node) {
        auto type = mapType(node->getType());
        auto name = node->getName();
        auto stackVar = createAlloca(type, name);
        auto value = node->getExpr()->gen(*this);
        builder.CreateStore(value, stackVar);
        namedValues[name] = stackVar;
        return nullptr;
    }

    llvm::Value *Codegen::gen(AssignNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        auto value = node->getExpr()->gen(*this);
        builder.CreateStore(value, var);
        return nullptr;
    }

    llvm::Value *Codegen::gen(VarNode *node) {
        auto name = node->getName();
        auto [type, var] = getVar(name);
        return builder.CreateLoad(type, var, name);
    }

    llvm::Value *Codegen::gen(IfNode *node) {
        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("if cond is empty");
        }
        cond = downcastToBool(cond);

        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto thenBB = llvm::BasicBlock::Create(context, "then", parentFunction);
        auto elseBB = llvm::BasicBlock::Create(context, "else");
        auto mergeBB = llvm::BasicBlock::Create(context, "ifcont");

        auto thenNode = node->getThenNode();
        auto elseNode = node->getElseNode();

        if (elseNode) {
            builder.CreateCondBr(cond, thenBB, elseBB);
        } else {
            builder.CreateCondBr(cond, thenBB, mergeBB);
        }

        builder.SetInsertPoint(thenBB);
        thenNode->gen(*this);
        if (!thenNode->isLastNodeTerminate()) {
            builder.CreateBr(mergeBB);
        }

        if (elseNode) {
            parentFunction->getBasicBlockList().push_back(elseBB);
            builder.SetInsertPoint(elseBB);
            elseNode->gen(*this);
            if (!elseNode->isLastNodeTerminate()) {
                builder.CreateBr(mergeBB);
            }
        }

        parentFunction->getBasicBlockList().push_back(mergeBB);
        builder.SetInsertPoint(mergeBB);

        return nullptr;
    }

    llvm::Value *Codegen::gen(WhileNode *node) {
        auto parentFunction = builder.GetInsertBlock()->getParent();
        auto loopStartBB = llvm::BasicBlock::Create(context, "loopStart");
        auto loopBB = llvm::BasicBlock::Create(context, "loop");
        auto loopEndBB = llvm::BasicBlock::Create(context, "loopEnd");

        loops.emplace(loopStartBB, loopEndBB);

        builder.CreateBr(loopStartBB);
        parentFunction->getBasicBlockList().push_back(loopStartBB);
        builder.SetInsertPoint(loopStartBB);

        auto cond = node->getCond()->gen(*this);
        if (!cond) {
            throw CodegenException("while cond is empty");
        }
        builder.CreateCondBr(cond, loopBB, loopEndBB);

        parentFunction->getBasicBlockList().push_back(loopBB);
        builder.SetInsertPoint(loopBB);
        node->getBody()->gen(*this);
        builder.CreateBr(loopStartBB);

        parentFunction->getBasicBlockList().push_back(loopEndBB);
        builder.SetInsertPoint(loopEndBB);

        loops.pop();

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnDeclNode *node) {
        genFn(node->getName(), node->getArgs(), node->getReturnType(), nullptr);

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnDefNode *node) {
        genFn(node->getName(), node->getArgs(), node->getReturnType(), node->getBody());

        return nullptr;
    }

    llvm::Value *Codegen::gen(FnCallNode *node) {
        auto name = node->getName();
        llvm::Function *callee = module.getFunction(name);
        if (!callee) {
            throw CodegenException("called function is not found: " + name);
        }
        if (callee->arg_size() != node->getArgs().size()) {
            throw CodegenException("callee args mismatch");
        }

        std::vector<llvm::Value *> args;
        for (auto arg: node->getArgs()) {
            args.push_back(arg->gen(*this));
        }

        return builder.CreateCall(callee, args);
    }

    llvm::Value *Codegen::gen(ReturnNode *node) {
        if (!node->getVal()) {
            builder.CreateRetVoid();
            return nullptr;
        }

        auto value = node->getVal()->gen(*this);
        if (!value) {
            throw CodegenException("return value is empty");
        }

        builder.CreateRet(value);
        return nullptr;
    }

    llvm::Value *Codegen::gen(BreakNode *node) {
        if (loops.empty()) {
            throw CodegenException("using break outside of loop");
        }

        auto loop = loops.top();
        builder.CreateBr(loop.end);

        return nullptr;
    }

    llvm::Value *Codegen::gen(ContinueNode *node) {
        if (loops.empty()) {
            throw CodegenException("using continue outside of loop");
        }

        auto loop = loops.top();
        builder.CreateBr(loop.start);

        return nullptr;
    }

    llvm::Value *Codegen::gen(CommentNode *node) {
        return nullptr;
    }

    llvm::Value *Codegen::gen(ClassNode *node) {
        auto name = node->getName();
        auto mangledName = mangler.mangleClass(name);
        auto members = node->getMembers();
        std::vector<llvm::Type *> props;
        props.reserve(members->getProps().size());
        ClassDecl classDecl;
        uint64_t propIndex = 0;

        if (!node->getParent().empty()) {
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

        checkInterfaces(node);

        return nullptr;
    }

    llvm::Value *Codegen::gen(FetchPropNode *node) {
        auto obj = node->getObj()->gen(*this);
        auto propName = node->getName();
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
        return builder.CreateAlloca(classDecl.type);
    }

    llvm::Value *Codegen::gen(InterfaceNode *node) {
        interfaces[node->getName()] = node;

        return nullptr;
    }

    llvm::Type *Codegen::mapType(const Type &type) const {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::FLOAT:
                return llvm::Type::getFloatTy(context);
            case Type::TypeID::BOOL:
                return llvm::Type::getInt1Ty(context);
            case Type::TypeID::VOID:
                return llvm::Type::getVoidTy(context);
            case Type::TypeID::CLASS: {
                auto className = type.getClassName();
                auto classDecl = getClass(mangler.mangleClass(className));
                return classDecl.type->getPointerTo();
            }
            default:
                throw CodegenException("invalid type");
        }
    }

    llvm::Constant *Codegen::getDefaultValue(const Type &type) const {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0);
            case Type::TypeID::FLOAT:
                return llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 0);
            case Type::TypeID::BOOL:
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            default:
                throw CodegenException("invalid type");
        }
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getVar(std::string &name) const {
        auto var = namedValues.find(name);
        if (var != namedValues.end()) {
            return {var->second->getAllocatedType(), var->second};
        }

        if (that) {
            auto classType = deref(that->getType());
            auto classDecl = getClass(classType->getStructName().str());
            auto prop = classDecl.props.find(name);
            if (prop != classDecl.props.end()) {
                auto ptr = builder.CreateStructGEP(classDecl.type, that, prop->second.pos);
                return {prop->second.type, ptr};
            }

            auto currentClassDecl = classDecl.parent;
            while (currentClassDecl) {
                prop = currentClassDecl->props.find(name);
                if (prop != currentClassDecl->props.end()) {
                    if (prop->second.accessModifier == AccessModifier::PRIVATE) {
                        throw CodegenException("cannot access private property: " + name);
                    }
                    auto parent = builder.CreateBitCast(that, currentClassDecl->type->getPointerTo());
                    auto ptr = builder.CreateStructGEP(currentClassDecl->type, parent, prop->second.pos);
                    return {prop->second.type, ptr};
                }

                currentClassDecl = currentClassDecl->parent;
            }
        }

        if (self) {
            auto currentClassDecl = self;
            while (currentClassDecl) {
                auto prop = currentClassDecl->staticProps.find(name);
                if (prop != currentClassDecl->staticProps.end()) {
                    if (currentClassDecl != self && prop->second.accessModifier == AccessModifier::PRIVATE) {
                        throw CodegenException("cannot access private property: " + name);
                    }
                    return {prop->second.var->getType()->getPointerElementType(), prop->second.var};
                }

                currentClassDecl = currentClassDecl->parent;
            }
        }

        throw CodegenException("var not found: " + name);
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

    llvm::AllocaInst *Codegen::createAlloca(llvm::Type *type, const std::string &name) const {
        auto fn = builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        return tmpBuilder.CreateAlloca(type, nullptr, name);
    }

    AccessModifier Codegen::getMethodAccessModifier(const std::string &mangledClassName, const std::string &methodName) const {
        auto classDecl = getClass(mangledClassName);
        auto method = classDecl.methods.find(methodName);
        if (method == classDecl.methods.end()) {
            throw CodegenException("method not found: " + methodName);
        }
        return method->second.accessModifier;
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::upcast(llvm::Value *a, llvm::Value *b) const {
        if (a->getType()->isFloatTy() && b->getType()->isIntegerTy()) {
            return {a, builder.CreateSIToFP(b, llvm::Type::getFloatTy(context))};
        }

        if (a->getType()->isIntegerTy() && b->getType()->isFloatTy()) {
            return {builder.CreateSIToFP(a, llvm::Type::getFloatTy(context)), b};
        }

        return {a, b};
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::forceUpcast(llvm::Value *a, llvm::Value *b) const {
        if (a->getType()->isIntegerTy()) {
            a = builder.CreateSIToFP(a, llvm::Type::getFloatTy(context));
        }

        if (b->getType()->isIntegerTy()) {
            b = builder.CreateSIToFP(b, llvm::Type::getFloatTy(context));
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
            case llvm::Type::TypeID::FloatTyID:
                return builder.CreateFCmpONE(value, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 0));
            default:
                throw CodegenException("invalid type");
        }
    }

    llvm::Type *Codegen::deref(llvm::Type *type) const {
        if (!type->isPointerTy()) {
            return type;
        }

        return type->getPointerElementType();
    }

    void Codegen::genFn(const std::string &name, const std::vector<ArgNode *> &args, const Type &returnType, StatementListNode *body, std::optional<Type> thisType) {
        size_t paramsOffset = thisType ? 1 : 0; // this is special
        std::vector<llvm::Type *> paramTypes;
        paramTypes.reserve(args.size() + paramsOffset);
        if (thisType) {
            paramTypes.push_back(mapType(thisType.value()));
        }
        for (auto arg: args) {
            paramTypes.push_back(mapType(arg->getType()));
        }

        auto retType = mapType(returnType);
        auto fnType = llvm::FunctionType::get(retType, paramTypes, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, name, module);
        if (thisType) {
            fn->getArg(0)->setName("this");
        }
        for (auto i = 0; i < args.size(); i++) {
            fn->getArg(i + paramsOffset)->setName(args[i]->getName());
        }

        if (!body) {
            return;
        }

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        if (thisType) {
            that = fn->getArg(0);
        }

        namedValues.clear(); // todo global vars
        for (auto i = paramsOffset; i < fn->arg_size(); i++) {
            auto arg = fn->getArg(i);
            auto argName = arg->getName().str();
            auto alloca = createAlloca(arg->getType(), argName);
            builder.CreateStore(arg, alloca);
            namedValues[argName] = alloca;
        }

        body->gen(*this);
        if (!body->isLastNodeTerminate()) {
            builder.CreateRetVoid();
        }

        that = nullptr;
    }

    void Codegen::checkInterfaces(ClassNode *classNode) const {
        auto classMethods = classNode->getMembers()->getMethods();

        for (auto &interfaceName: classNode->getInterfaces()) {
            auto interface = interfaces.find(interfaceName);
            if (interface == interfaces.end()) {
                throw CodegenException(fmt::format("interface {} not found", interfaceName));
            }

            for (auto interfaceMethod: interface->second->getMethods()) {
                auto classMethod = std::find_if(classMethods.cbegin(), classMethods.cend(), [interfaceMethod](MethodDefNode *method) {
                    return method->getFnDef()->getName() == interfaceMethod->getFnDecl()->getName();
                });

                if (classMethod == classMethods.cend()) {
                    throw CodegenException(fmt::format("interface method {}::{} must be implemented", interfaceName, interfaceMethod->getFnDecl()->getName()));
                }

                if (!compareDeclAndDef(interfaceMethod, *classMethod)) {
                    throw CodegenException(
                            fmt::format("declaration of {}::{} must be compatible with interface {}", classNode->getName(), (*classMethod)->getFnDef()->getName(), interfaceName));
                }
            }
        }
    }

    bool Codegen::compareDeclAndDef(MethodDeclNode *declNode, MethodDefNode *defNode) const {
        if (declNode->getAccessModifier() != defNode->getAccessModifier()) {
            return false;
        }

        if (declNode->getIsStatic() != defNode->getIsStatic()) {
            return false;
        }

        auto fnDecl = declNode->getFnDecl();
        auto fnDef = defNode->getFnDef();

        if (fnDecl->getReturnType() != fnDef->getReturnType()) {
            return false;
        }

        if (fnDecl->getArgs().size() != fnDef->getArgs().size()) {
            return false;
        }

        for (auto i = 0; i < fnDecl->getArgs().size(); i++) {
            if (fnDecl->getArgs()[i]->getType() != fnDef->getArgs()[i]->getType()) {
                return false;
            }
        }

        return true;
    }

    std::pair<llvm::Function *, llvm::Type *> Codegen::findMethod(llvm::StructType *type, const std::string &methodName) const {
        const ClassDecl *currentClassDecl = &getClass(type->getStructName().str());
        while (currentClassDecl) {
            auto className = currentClassDecl->type->getStructName();
            auto name = mangler.mangleMethod(className.str(), methodName);
            auto callee = module.getFunction(name);
            if (callee) {
                auto methodAccessModifier = getMethodAccessModifier(className.str(), methodName);
                if (!that && methodAccessModifier != AccessModifier::PUBLIC) {
                    throw CodegenException("cannot access private method: " + methodName);
                }
                return {callee, currentClassDecl->type};
            }

            currentClassDecl = currentClassDecl->parent;
        }

        return {nullptr, nullptr};
    }
}
