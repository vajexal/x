#include "codegen.h"

namespace X {
    llvm::Value *Codegen::gen(Node *node) {
        throw CodegenException("can't gen node");
    }

    llvm::Value *Codegen::gen(StatementListNode *node) {
        for (auto &child: node->getChildren()) {
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
        auto expr = node->getExpr()->gen(*this);

        switch (node->getType()) {
            case OpType::POST_INC: {
                llvm::Value *value;
                switch (expr->getType()->getTypeID()) {
                    case llvm::Type::TypeID::IntegerTyID:
                        value = builder.CreateAdd(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
                        break;
                    case llvm::Type::TypeID::FloatTyID:
                        value = builder.CreateFAdd(expr, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 1));
                        break;
                    default:
                        throw CodegenException("invalid type");
                }

                auto name = dynamic_cast<VarNode *>(node->getExpr())->getName(); // todo
                auto [type, var] = getVar(name);
                builder.CreateStore(value, var);
                return expr;
            }
            case OpType::POST_DEC: {
                llvm::Value *value;
                switch (expr->getType()->getTypeID()) {
                    case llvm::Type::TypeID::IntegerTyID:
                        value = builder.CreateSub(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
                        break;
                    case llvm::Type::TypeID::FloatTyID:
                        value = builder.CreateFSub(expr, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 1));
                        break;
                    default:
                        throw CodegenException("invalid type");
                }

                auto name = dynamic_cast<VarNode *>(node->getExpr())->getName(); // todo
                auto [type, var] = getVar(name);
                builder.CreateStore(value, var);
                return expr;
            }
            case OpType::NOT:
                // todo true binary op
                return builder.CreateXor(expr, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 1));
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
        if (!cond->getType()->isIntegerTy(1)) {
            switch (cond->getType()->getTypeID()) {
                case llvm::Type::TypeID::IntegerTyID:
                    cond = builder.CreateICmpNE(cond, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), 0));
                    break;
                case llvm::Type::TypeID::FloatTyID:
                    cond = builder.CreateFCmpONE(cond, llvm::ConstantFP::get(llvm::Type::getFloatTy(context), 0));
                    break;
                default:
                    throw CodegenException("invalid cond type in if expr");
            }
        }

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

    llvm::Value *Codegen::gen(FnNode *node) {
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
        for (auto &arg: node->getArgs()) {
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
        auto members = node->getMembers();
        std::vector<llvm::Type *> props;
        props.reserve(members.getProps().size());
        ClassDecl classDecl;

        for (uint64_t i = 0; i < members.getProps().size(); i++) {
            auto prop = members.getProps()[i];
            auto type = mapType(prop->getType());
            props.push_back(type);
            classDecl.props[prop->getName()] = {type, i};
        }

        auto name = node->getName();
        auto klass = llvm::StructType::create(context, props, name); // todo mangle name
        classDecl.type = klass;
        classes[name] = classDecl;

        for (auto &fn: members.getFuncs()) {
            auto fnName = name + "_" + fn->getName(); // todo mangle
            genFn(fnName, fn->getArgs(), fn->getReturnType(), fn->getBody(), std::move(Type(name)));
        }

        return nullptr;
    }

    llvm::Value *Codegen::gen(ClassMembersNode *node) {
        // everything is done in ClassNode
        return nullptr;
    }

    llvm::Value *Codegen::gen(FetchPropNode *node) {
        auto obj = node->getObj()->gen(*this);
        auto propName = node->getName();
        auto [type, ptr] = getProp(obj, propName);
        return builder.CreateLoad(type, ptr, propName);
    }

    llvm::Value *Codegen::gen(MethodCallNode *node) {
        auto obj = node->getObj()->gen(*this);
        auto methodName = node->getName();
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw CodegenException("invalid method call operand");
        }

        auto className = type->getStructName();
        auto classDecl = getClass(className.str());
        auto name = className.str() + "_" + methodName; // todo mangle
        llvm::Function *callee = module.getFunction(name);
        if (!callee) {
            throw CodegenException("method is not found: " + methodName);
        }
        if (callee->arg_size() != (node->getArgs().size() + 1)) { // + this
            throw CodegenException("callee args mismatch");
        }

        std::vector<llvm::Value *> args;
        args.reserve(node->getArgs().size() + 1);
        args.push_back(obj);
        for (auto &arg: node->getArgs()) {
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

    llvm::Value *Codegen::gen(NewNode *node) {
        auto classDecl = getClass(node->getName());
        return builder.CreateAlloca(classDecl.type);
    }

    llvm::Type *Codegen::mapType(const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::FLOAT:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::BOOL:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::VOID:
                return llvm::Type::getInt64Ty(context);
            case Type::TypeID::CLASS: {
                auto className = type.getClassName().value();
                auto classDecl = getClass(className);
                return classDecl.type->getPointerTo();
            }
            default:
                throw CodegenException("invalid type");
        }
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getVar(std::string &name) {
        auto var = namedValues[name];
        if (var) {
            return {var->getAllocatedType(), var};
        }

        if (that) {
            auto classType = deref(that->getType());
            auto classDecl = getClass(classType->getStructName().str());
            auto prop = classDecl.props.find(name);
            if (prop != classDecl.props.end()) {
                auto ptr = builder.CreateStructGEP(classDecl.type, that, prop->second.pos);
                return {prop->second.type, ptr};
            }
        }

        throw CodegenException("var not found: " + name);
    }

    std::pair<llvm::Type *, llvm::Value *> Codegen::getProp(llvm::Value *obj, std::string &name) {
        auto type = deref(obj->getType());
        if (!type->isStructTy()) {
            throw CodegenException("invalid obj operand");
        }

        auto className = type->getStructName();
        auto classDecl = getClass(className.str());
        auto prop = classDecl.props.find(name);
        if (prop == classDecl.props.end()) {
            throw CodegenException("prop not found: " + name);
        }

        auto ptr = builder.CreateStructGEP(classDecl.type, obj, prop->second.pos);
        return {prop->second.type, ptr};
    }

    const ClassDecl &Codegen::getClass(const std::string &name) const {
        auto classDecl = classes.find(name); // todo mangle
        if (classDecl == classes.end()) {
            throw CodegenException("class not found: " + name);
        }
        return classDecl->second;
    }

    llvm::AllocaInst *Codegen::createAlloca(llvm::Type *type, const std::string &name) {
        auto fn = builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        return tmpBuilder.CreateAlloca(type, nullptr, name);
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::upcast(llvm::Value *a, llvm::Value *b) {
        if (a->getType()->isFloatTy() && b->getType()->isIntegerTy()) {
            return {a, builder.CreateSIToFP(b, llvm::Type::getFloatTy(context))};
        }

        if (a->getType()->isIntegerTy() && b->getType()->isFloatTy()) {
            return {builder.CreateSIToFP(a, llvm::Type::getFloatTy(context)), b};
        }

        return {a, b};
    }

    std::pair<llvm::Value *, llvm::Value *> Codegen::forceUpcast(llvm::Value *a, llvm::Value *b) {
        if (a->getType()->isIntegerTy()) {
            a = builder.CreateSIToFP(a, llvm::Type::getFloatTy(context));
        }

        if (b->getType()->isIntegerTy()) {
            b = builder.CreateSIToFP(b, llvm::Type::getFloatTy(context));
        }

        return {a, b};
    }

    llvm::Type *Codegen::deref(llvm::Type *type) {
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
        for (auto &arg: args) {
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

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        if (thisType) {
            that = fn->getArg(0);
        }

        namedValues.clear(); // todo
        for (auto i = paramsOffset; i < fn->arg_size(); i++) {
            auto arg = fn->getArg(i);
            auto argName = arg->getName().str();
            auto alloca = createAlloca(arg->getType(), argName);
            builder.CreateStore(arg, alloca);
            namedValues[argName] = alloca;
        }

        body->gen(*this);

        that = nullptr;
    }
}
