#include "type_inferrer.h"

#include <ranges>
#include <fmt/core.h>

#include "runtime/runtime.h"
#include "utils.h"

namespace X::Pipes {
    TopStatementListNode *TypeInferrer::handle(TopStatementListNode *node) {
        addRuntime();
        declClasses(node);
        declMethods(node);
        declFuncs(node);
        declGlobals(node);

        infer(node);

        compilerRuntime->fnTypes = std::move(fnTypes);
        compilerRuntime->classMethodTypes = std::move(classMethodTypes);

        return node;
    }

    void TypeInferrer::addRuntime() {
        fnTypes["exit"] = {{}, Type::voidTy()};

        classMethodTypes[Runtime::String::CLASS_NAME].insert({
                                                                     {"concat", {{{Type::scalar(Type::TypeID::STRING)}, Type::scalar(Type::TypeID::STRING)}}},
                                                                     {"length", {{{}, Type::scalar(Type::TypeID::INT)}}},
                                                                     {"isEmpty", {{{}, Type::scalar(Type::TypeID::BOOL)}}},
                                                                     {"trim", {{{}, Type::scalar(Type::TypeID::STRING)}}},
                                                                     {"toLower", {{{}, Type::scalar(Type::TypeID::STRING)}}},
                                                                     {"toUpper", {{{}, Type::scalar(Type::TypeID::STRING)}}},
                                                                     {"index", {{{Type::scalar(Type::TypeID::STRING)}, Type::scalar(Type::TypeID::INT)}}},
                                                                     {"contains", {{{Type::scalar(Type::TypeID::STRING)}, Type::scalar(Type::TypeID::BOOL)}}},
                                                                     {"startsWith", {{{Type::scalar(Type::TypeID::STRING)}, Type::scalar(Type::TypeID::BOOL)}}},
                                                                     {"endsWith", {{{Type::scalar(Type::TypeID::STRING)}, Type::scalar(Type::TypeID::BOOL)}}},
                                                                     {"substring", {{{Type::scalar(Type::TypeID::INT), Type::scalar(Type::TypeID::INT)},
                                                                                     Type::scalar(Type::TypeID::STRING)}}},
                                                             });

        classMethodTypes[Runtime::ArrayRuntime::CLASS_NAME].insert({
                                                                           {"length", {{{}, Type::scalar(Type::TypeID::INT)}}},
                                                                           {"isEmpty", {{{}, Type::scalar(Type::TypeID::BOOL)}}},
                                                                   });
    }

    void TypeInferrer::declClasses(TopStatementListNode *node) {
        for (auto klass: node->classes) {
            classes.insert(klass->name);
        }

        for (auto klass: node->classes) {
            auto &name = klass->name;

            classProps[name] = {};

            if (klass->hasParent()) {
                classProps[name] = classProps[klass->parent];
            }

            auto &props = classProps[name];

            for (auto prop: klass->props) {
                auto decl = prop->decl;

                checkDecl(decl);

                props[decl->name] = {decl->type, prop->isStatic};
            }
        }
    }

    void TypeInferrer::declMethods(TopStatementListNode *node) {
        for (auto klass: node->classes) {
            auto &name = klass->name;

            if (klass->hasParent()) {
                classMethodTypes[name] = classMethodTypes[klass->parent];
            }

            auto &classMethods = classMethodTypes[name];

            for (auto &[methodName, methodDecl]: klass->abstractMethods) {
                auto fnDecl = methodDecl->fnDecl;
                std::vector<Type> args;
                args.reserve(fnDecl->args.size());

                for (auto arg: fnDecl->args) {
                    args.push_back(arg->infer(*this));
                }

                auto &retType = getMethodReturnType(fnDecl, name);
                checkTypeIsValid(retType);
                classMethods[fnDecl->name] = {{args, retType}, methodDecl->isStatic};
            }

            for (auto &[methodName, methodDef]: klass->methods) {
                auto fnDecl = methodDef->fnDef->decl;
                std::vector<Type> args;
                args.reserve(fnDecl->args.size());

                for (auto arg: fnDecl->args) {
                    args.push_back(arg->infer(*this));
                }

                auto &retType = getMethodReturnType(fnDecl, name);
                checkTypeIsValid(retType);
                classMethods[methodName] = {{args, retType}, methodDef->isStatic};
            }

            if (!classMethods.contains(CONSTRUCTOR_FN_NAME)) {
                classMethods[CONSTRUCTOR_FN_NAME] = {{{}, Type::voidTy()}};
            }
        }
    }

    void TypeInferrer::declFuncs(TopStatementListNode *node) {
        for (auto fnDef: node->funcs) {
            auto fnDecl = fnDef->decl;

            std::vector<Type> args;
            args.reserve(fnDecl->args.size());

            for (auto arg: fnDecl->args) {
                args.push_back(arg->infer(*this));
            }

            auto &retType = fnDecl->returnType;
            checkTypeIsValid(retType);
            fnTypes[fnDecl->name] = {args, retType};
        }
    }

    void TypeInferrer::declGlobals(TopStatementListNode *node) {
        auto &globals = node->globals;

        if (!globals.empty()) {
            varScopes.emplace_back();
        }

        for (auto decl: globals) {
            decl->infer(*this);
        }
    }

    Type TypeInferrer::infer(Node *node) {
        throw TypeInferrerException("can't infer node");
    }

    Type TypeInferrer::infer(StatementListNode *node) {
        for (auto child: node->children) {
            child->infer(*this);
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(ScalarNode *node) {
        // check all elements have the same type
        if (node->type.is(Type::TypeID::ARRAY)) {
            auto &exprList = std::get<ExprList>(node->value);
            auto firstExprType = exprList[0]->infer(*this);

            if (firstExprType.is(Type::TypeID::VOID)) {
                throw TypeInferrerException("array must not have void elements");
            }

            if (!std::ranges::all_of(exprList.begin() + 1, exprList.end(), [&](auto expr) { return expr->infer(*this) == firstExprType; })) {
                throw TypeInferrerException("all array elements must be the same type");
            }

            return Type::array(std::move(firstExprType));
        }

        return node->type;
    }

    Type TypeInferrer::infer(UnaryNode *node) {
        auto exprType = node->expr->infer(*this);

        switch (node->opType) {
            case OpType::PRE_INC:
            case OpType::PRE_DEC:
            case OpType::POST_INC:
            case OpType::POST_DEC:
                if (!exprType.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT)) {
                    throw InvalidTypeException();
                }

                return exprType;
            case OpType::NOT:
                if (exprType.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS)) {
                    throw InvalidTypeException();
                }

                return Type::scalar(Type::TypeID::BOOL);
            default:
                throw TypeInferrerException("invalid op type");
        }
    }

    Type TypeInferrer::infer(BinaryNode *node) {
        auto lhs = node->lhs->infer(*this);
        auto rhs = node->rhs->infer(*this);

        switch (node->opType) {
            case OpType::OR:
            case OpType::AND:
                if (lhs.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS) || rhs.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS)) {
                    throw InvalidTypeException();
                }

                return Type::scalar(Type::TypeID::BOOL);
            case OpType::EQUAL:
            case OpType::NOT_EQUAL:
                if (lhs.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS) || rhs.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS)) {
                    throw InvalidTypeException();
                }

                if (lhs != rhs) {
                    throw TypeInferrerException("incompatible types");
                }

                return Type::scalar(Type::TypeID::BOOL);
            case OpType::PLUS:
            case OpType::MINUS:
                if (lhs.is(Type::TypeID::STRING) && rhs.is(Type::TypeID::STRING)) {
                    return Type::scalar(Type::TypeID::STRING);
                }

                if (lhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT) && rhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT)) {
                    if (lhs.is(Type::TypeID::FLOAT) || rhs.is(Type::TypeID::FLOAT)) {
                        return Type::scalar(Type::TypeID::FLOAT);
                    }

                    return Type::scalar(Type::TypeID::INT);
                }

                throw InvalidTypeException();
            case OpType::MUL:
            case OpType::MOD:
                if (lhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT) && rhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT)) {
                    if (lhs.is(Type::TypeID::FLOAT) || rhs.is(Type::TypeID::FLOAT)) {
                        return Type::scalar(Type::TypeID::FLOAT);
                    }

                    return Type::scalar(Type::TypeID::INT);
                }

                throw InvalidTypeException();
            case OpType::DIV:
            case OpType::POW:
                if (lhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT) && rhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT)) {
                    return Type::scalar(Type::TypeID::FLOAT);
                }

                throw InvalidTypeException();
            case OpType::SMALLER:
            case OpType::SMALLER_OR_EQUAL:
            case OpType::GREATER:
            case OpType::GREATER_OR_EQUAL:
                if (lhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT) && rhs.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT)) {
                    return Type::scalar(Type::TypeID::BOOL);
                }

                throw InvalidTypeException();
            default:
                throw TypeInferrerException("invalid op type");
        }
    }

    Type TypeInferrer::infer(DeclNode *node) {
        checkDecl(node);

        varScopes.back()[node->name] = node->type;

        return Type::voidTy();
    }

    Type TypeInferrer::infer(AssignNode *node) {
        auto exprType = node->expr->infer(*this);
        auto varType = getVarType(node->name);

        if (!canCastTo(exprType, varType)) {
            throw InvalidTypeException();
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(VarNode *node) {
        auto &name = node->name;
        if (name == THIS_KEYWORD && that) {
            return Type::klass(that.value());
        }

        return getVarType(name);
    }

    Type TypeInferrer::infer(IfNode *node) {
        auto condType = node->cond->infer(*this);
        if (condType.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS)) {
            throw InvalidTypeException();
        }

        node->thenNode->infer(*this);
        if (node->elseNode) {
            node->elseNode->infer(*this);
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(WhileNode *node) {
        auto condType = node->cond->infer(*this);
        if (condType.isOneOf(Type::TypeID::VOID, Type::TypeID::CLASS)) {
            throw InvalidTypeException();
        }

        node->body->infer(*this);

        return Type::voidTy();
    }

    Type TypeInferrer::infer(ForNode *node) {
        auto exprType = node->expr->infer(*this);

        // for expression must be array or range
        if (!exprType.is(Type::TypeID::ARRAY)) {
            throw TypeInferrerException("for expression must be array or range");
        }

        varScopes.emplace_back();
        auto &vars = varScopes.back();

        if (node->idx) {
            vars[node->idx.value()] = Type::scalar(Type::TypeID::INT);
        }
        const auto &varType = exprType.is(Type::TypeID::ARRAY) ? *exprType.getSubtype() : Type::scalar(Type::TypeID::INT);
        vars[node->val] = varType;

        node->body->infer(*this);

        varScopes.pop_back();

        return Type::voidTy();
    }

    Type TypeInferrer::infer(RangeNode *node) {
        auto startType = node->start ? node->start->infer(*this) : Type::scalar(Type::TypeID::INT);
        auto stopType = node->stop->infer(*this);
        auto stepType = node->step ? node->step->infer(*this) : Type::scalar(Type::TypeID::INT);

        if (!startType.is(Type::TypeID::INT)) {
            throw TypeInferrerException("range start argument must be int");
        }
        if (!stopType.is(Type::TypeID::INT)) {
            throw TypeInferrerException("range stop argument must be int");
        }
        if (!stepType.is(Type::TypeID::INT)) {
            throw TypeInferrerException("range step argument must be int");
        }

        return Type::array(Type::scalar(Type::TypeID::INT));
    }

    Type TypeInferrer::infer(BreakNode *node) {
        return Type::voidTy();
    }

    Type TypeInferrer::infer(ContinueNode *node) {
        return Type::voidTy();
    }

    Type TypeInferrer::infer(ArgNode *node) {
        auto &type = node->type;

        checkArgTypeIsValid(type);

        return type;
    }

    Type TypeInferrer::infer(FnDeclNode *node) {
        std::vector<Type> args;
        args.reserve(node->args.size());

        for (auto arg: node->args) {
            args.push_back(arg->infer(*this));
        }

        auto &retType = node->returnType;
        checkTypeIsValid(retType);
        fnTypes[node->name] = {args, retType};

        return Type::voidTy();
    }

    Type TypeInferrer::infer(FnDefNode *node) {
        varScopes.emplace_back();
        auto &vars = varScopes.back();
        for (auto arg: node->decl->args) {
            vars[arg->name] = arg->infer(*this);
        }

        currentFnRetType = node->decl->returnType;

        node->body->infer(*this);

        varScopes.pop_back();

        return Type::voidTy();
    }

    Type TypeInferrer::infer(FnCallNode *node) {
        auto &fnType = getFnType(node->name);
        checkFnCall(fnType, node->args);
        return fnType.retType;
    }

    Type TypeInferrer::infer(ReturnNode *node) {
        if (!node->val) {
            return Type::voidTy();
        }

        auto retType = node->val->infer(*this);

        if (retType.is(Type::TypeID::VOID)) {
            throw InvalidTypeException();
        }

        if (!canCastTo(retType, currentFnRetType)) {
            throw InvalidTypeException();
        }

        return std::move(retType);
    }

    Type TypeInferrer::infer(PrintlnNode *node) {
        auto type = node->val->infer(*this);

        if (type.is(Type::TypeID::ARRAY)) {
            type = *type.getSubtype();
        }

        if (!isPrintable(type)) {
            throw InvalidTypeException();
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(CommentNode *node) {
        return Type::voidTy();
    }

    Type TypeInferrer::infer(ClassNode *node) {
        // all we need to do is infer method bodies
        self = node->name;

        for (auto &[_, methodDef]: node->methods) {
            methodDef->infer(*this);
        }

        self.reset();

        return Type::voidTy();
    }

    Type TypeInferrer::infer(MethodDefNode *node) {
        auto fnDecl = node->fnDef->decl;

        if (!node->isStatic) {
            that = self;
        }

        std::vector<Type> args;
        args.reserve(fnDecl->args.size());

        varScopes.emplace_back();
        auto &vars = varScopes.back();
        for (auto arg: fnDecl->args) {
            vars[arg->name] = arg->infer(*this);
        }

        currentFnRetType = fnDecl->returnType;

        node->fnDef->body->infer(*this);

        varScopes.pop_back();

        that.reset();

        return Type::voidTy();
    }

    Type TypeInferrer::infer(FetchPropNode *node) {
        auto objType = node->obj->infer(*this);
        if (!objType.is(Type::TypeID::CLASS)) {
            throw InvalidTypeException();
        }

        return getPropType(objType.getClassName(), node->name);
    }

    Type TypeInferrer::infer(FetchStaticPropNode *node) {
        return getPropType(node->className, node->propName, true);
    }

    Type TypeInferrer::infer(MethodCallNode *node) {
        auto objType = node->obj->infer(*this);
        const auto &className = getObjectClassName(objType);

        auto &methodType = getMethodType(className, node->name);
        checkFnCall(methodType, node->args);
        return methodType.retType;
    }

    Type TypeInferrer::infer(StaticMethodCallNode *node) {
        auto &methodType = getMethodType(node->className, node->methodName, true);
        checkFnCall(methodType, node->args);
        return methodType.retType;
    }

    Type TypeInferrer::infer(AssignPropNode *node) {
        auto objType = node->obj->infer(*this);
        if (!objType.is(Type::TypeID::CLASS)) {
            throw InvalidTypeException();
        }

        auto &propType = getPropType(objType.getClassName(), node->name);
        auto exprType = node->expr->infer(*this);

        if (!canCastTo(exprType, propType)) {
            throw InvalidTypeException();
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(AssignStaticPropNode *node) {
        auto &propType = getPropType(node->className, node->propName, true);
        auto exprType = node->expr->infer(*this);

        if (!canCastTo(exprType, propType)) {
            throw InvalidTypeException();
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(NewNode *node) {
        auto &name = node->name;
        // need to check class existence here, otherwise it's possible to get type error with non-existent class
        if (!classes.contains(name)) {
            throw TypeInferrerException(fmt::format("class {} not found", name));
        }

        auto &methodType = getMethodType(name, CONSTRUCTOR_FN_NAME);
        checkFnCall(methodType, node->args);
        return Type::klass(name);
    }

    Type TypeInferrer::infer(MethodDeclNode *node) {
        auto fnDecl = node->fnDecl;

        std::vector<Type> args;
        args.reserve(fnDecl->args.size());

        for (auto arg: fnDecl->args) {
            args.push_back(arg->infer(*this));
        }

        auto &retType = getMethodReturnType(fnDecl, self.value());
        checkTypeIsValid(retType);
        classMethodTypes[self.value()][fnDecl->name] = {{args, retType}, node->isStatic};

        return Type::voidTy();
    }

    Type TypeInferrer::infer(InterfaceNode *node) {
        self = node->name;

        node->body->infer(*this);

        self.reset();

        return Type::voidTy();
    }

    Type TypeInferrer::infer(FetchArrNode *node) {
        auto arrType = node->arr->infer(*this);
        if (!arrType.is(Type::TypeID::ARRAY)) {
            throw InvalidTypeException();
        }

        auto idxType = node->idx->infer(*this);
        if (!idxType.is(Type::TypeID::INT)) {
            throw InvalidTypeException();
        }

        return *arrType.getSubtype();
    }

    Type TypeInferrer::infer(AssignArrNode *node) {
        auto arrType = node->arr->infer(*this);
        if (!arrType.is(Type::TypeID::ARRAY)) {
            throw InvalidTypeException();
        }

        auto idxType = node->idx->infer(*this);
        if (!idxType.is(Type::TypeID::INT)) {
            throw InvalidTypeException();
        }

        auto exprType = node->expr->infer(*this);
        if (!canCastTo(exprType, *arrType.getSubtype())) {
            throw InvalidTypeException();
        }

        return Type::voidTy();
    }

    Type TypeInferrer::infer(AppendArrNode *node) {
        auto arrType = node->arr->infer(*this);
        if (!arrType.is(Type::TypeID::ARRAY)) {
            throw InvalidTypeException();
        }

        auto exprType = node->expr->infer(*this);
        if (!canCastTo(exprType, *arrType.getSubtype())) {
            throw InvalidTypeException();
        }

        return Type::voidTy();
    }

    void TypeInferrer::checkTypeIsValid(const Type &type) const {
        if (type.is(Type::TypeID::ARRAY) && type.getSubtype()->is(Type::TypeID::VOID)) {
            throw InvalidTypeException();
        }

        if (type.isOneOf(Type::TypeID::AUTO, Type::TypeID::SELF)) {
            throw InvalidTypeException();
        }
    }

    void TypeInferrer::checkLvalueTypeIsValid(const Type &type) const {
        if (type.is(Type::TypeID::VOID)) {
            throw InvalidTypeException();
        }

        if (type.is(Type::TypeID::CLASS) && !classes.contains(type.getClassName())) {
            throw InvalidTypeException();
        }

        checkTypeIsValid(type);
    }

    void TypeInferrer::checkArgTypeIsValid(const Type &type) const {
        if (type.is(Type::TypeID::VOID)) {
            throw InvalidTypeException();
        }

        checkTypeIsValid(type);
    }

    void TypeInferrer::checkDecl(DeclNode *node) {
        auto &type = node->type;

        if (type.is(Type::TypeID::AUTO)) {
            // infer expr type and change decl type accordingly
            if (!node->expr) {
                throw InvalidTypeException();
            }

            auto exprType = node->expr->infer(*this);

            checkLvalueTypeIsValid(exprType);

            node->type = exprType;

            return;
        }

        checkLvalueTypeIsValid(type);

        if (node->expr) {
            auto exprType = node->expr->infer(*this);

            if (!canCastTo(exprType, type)) {
                throw InvalidTypeException();
            }
        }
    }

    void TypeInferrer::checkFnCall(const FnType &fnType, const ExprList &args) {
        if (fnType.args.size() != args.size()) {
            throw TypeInferrerException("call args mismatch");
        }

        for (auto i = 0; i < fnType.args.size(); i++) {
            auto argType = args[i]->infer(*this);
            if (!canCastTo(argType, fnType.args[i])) {
                throw InvalidTypeException();
            }
        }
    }

    const Type &TypeInferrer::getMethodReturnType(FnDeclNode *fnDecl, const std::string &className) const {
        if (fnDecl->returnType.is(Type::TypeID::SELF)) {
            fnDecl->returnType = Type::klass(className);
        }

        return fnDecl->returnType;
    }

    Type TypeInferrer::getVarType(const std::string &name) const {
        // most nested scope will be last, so search in reverse order
        for (auto &vars: std::ranges::reverse_view(varScopes)) {
            auto var = vars.find(name);
            if (var != vars.cend()) {
                return var->second;
            }
        }

        if (self) {
            auto &props = classProps.at(self.value());
            auto propIt = props.find(name);
            if (propIt != props.cend() && (that || propIt->second.isStatic)) {
                return propIt->second.type;
            }
        }

        throw TypeInferrerException(fmt::format("var {} not found", name));
    }

    const FnType &TypeInferrer::getFnType(const std::string &fnName) const {
        auto fnTypeIt = fnTypes.find(fnName);
        if (fnTypeIt != fnTypes.cend()) {
            return fnTypeIt->second;
        }

        if (self) {
            const auto &methods = classMethodTypes.at(self.value());
            auto methodIt = methods.find(fnName);
            if (methodIt != methods.cend() && (that || methodIt->second.isStatic)) {
                return methodIt->second;
            }
        }

        throw TypeInferrerException(fmt::format("fn {} not found", fnName));
    }

    const MethodType &TypeInferrer::getMethodType(const std::string &className, const std::string &methodName, bool isStatic) const {
        const auto &methodTypesIt = classMethodTypes.find(className);
        if (methodTypesIt == classMethodTypes.cend()) {
            throw TypeInferrerException(fmt::format("class {} not found", className));
        }

        auto methodTypeIt = methodTypesIt->second.find(methodName);
        if (methodTypeIt == methodTypesIt->second.cend()) {
            throw TypeInferrerException(fmt::format("method {}::{} not found", className, methodName));
        }

        if (methodTypeIt->second.isStatic != isStatic) {
            throw TypeInferrerException(fmt::format("wrong method call {}::{}", className, methodName));
        }

        return methodTypeIt->second;
    }

    const Type &TypeInferrer::getPropType(const std::string &className, const std::string &propName, bool isStatic) const {
        auto &klassName = isStatic && className == SELF_KEYWORD && self ? self.value() : className;

        auto classPropsIt = classProps.find(klassName);
        if (classPropsIt == classProps.cend()) {
            throw TypeInferrerException(fmt::format("class {} not found", klassName));
        }

        auto propTypeIt = classPropsIt->second.find(propName);
        if (propTypeIt == classPropsIt->second.cend()) {
            throw TypeInferrerException(fmt::format("prop {}::{} not found", klassName, propName));
        }

        if (propTypeIt->second.isStatic != isStatic) {
            throw TypeInferrerException(fmt::format("wrong prop access {}::{}", klassName, propName));
        }

        return propTypeIt->second.type;
    }

    std::string TypeInferrer::getObjectClassName(const Type &objType) const {
        switch (objType.getTypeID()) {
            case Type::TypeID::CLASS:
                return objType.getClassName();
            case Type::TypeID::STRING:
                return Runtime::String::CLASS_NAME;
            case Type::TypeID::ARRAY:
                return Runtime::ArrayRuntime::CLASS_NAME;
            default:
                throw InvalidTypeException();
        }
    }

    bool TypeInferrer::canCastTo(const Type &type, const Type &expectedType) const {
        if (type == expectedType) {
            return true;
        }

        if (type.is(Type::TypeID::INT) && expectedType.is(Type::TypeID::FLOAT)) {
            return true;
        }

        if (type.is(Type::TypeID::CLASS) && expectedType.is(Type::TypeID::CLASS) && instanceof(type, expectedType)) {
            return true;
        }

        return false;
    }

    bool TypeInferrer::instanceof(const Type &instanceType, const Type &type) const {
        return compilerRuntime->extendedClasses[instanceType.getClassName()].contains(type.getClassName()) ||
               compilerRuntime->implementedInterfaces[instanceType.getClassName()].contains(type.getClassName());
    }

    bool TypeInferrer::isPrintable(const Type &type) const {
        return type.isOneOf(Type::TypeID::INT, Type::TypeID::FLOAT, Type::TypeID::BOOL, Type::TypeID::STRING);
    }
}
