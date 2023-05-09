#include "ast.h"

#include "pipes/print_ast.h"
#include "codegen/codegen.h"
#include "pipes/type_inferrer.h"
#include "utils.h"

namespace X {
    Type::Type(const Type &type) : id(type.id) {
        if (type.className) {
            className = type.className.value();
        }

        if (type.subtype) {
            subtype = new Type(*type.subtype);
        }
    }

    Type::Type(Type &&type) : id(type.id), className(std::move(type.className)), subtype(type.subtype) {
        type.id = TypeID::VOID;
        type.className = std::nullopt;
        type.subtype = nullptr;
    }

    Type &Type::operator=(const Type &type) {
        id = type.id;

        if (type.className) {
            className = type.className.value();
        }

        if (type.subtype) {
            subtype = new Type(*type.subtype);
        }

        return *this;
    }

    Type::~Type() {
        delete subtype;
    }

    Type Type::scalar(Type::TypeID typeID) {
        if (typeID == TypeID::CLASS || typeID == TypeID::ARRAY || typeID == TypeID::AUTO || typeID == TypeID::SELF) {
            throw std::invalid_argument("invalid type for scalar");
        }

        Type type;
        type.id = typeID;

        return std::move(type);
    }

    Type Type::klass(std::string className) {
        Type type;
        type.id = TypeID::CLASS;
        type.className = std::move(className);

        return std::move(type);
    }

    Type Type::array(Type &&subtype) {
        Type type;
        type.id = TypeID::ARRAY;
        type.subtype = new Type(std::move(subtype));;

        return std::move(type);
    }

    Type Type::voidTy() {
        Type type;
        type.id = TypeID::VOID;

        return std::move(type);
    }

    Type Type::autoTy() {
        Type type;
        type.id = TypeID::AUTO;

        return std::move(type);
    }

    Type Type::selfTy() {
        Type type;
        type.id = TypeID::SELF;

        return std::move(type);
    }

    std::ostream &operator<<(std::ostream &out, OpType type) {
        switch (type) {
            case OpType::PRE_INC: return out << "++ ";
            case OpType::PRE_DEC: return out << "-- ";
            case OpType::POST_INC: return out << " ++";
            case OpType::POST_DEC: return out << " --";
            case OpType::OR: return out << "||";
            case OpType::AND: return out << "&&";
            case OpType::PLUS: return out << "+";
            case OpType::MINUS: return out << "-";
            case OpType::MUL: return out << "*";
            case OpType::DIV: return out << "/";
            case OpType::NOT: return out << "!";
            case OpType::POW: return out << "**";
            case OpType::MOD: return out << "%";
            case OpType::EQUAL: return out << "==";
            case OpType::NOT_EQUAL: return out << "!=";
            case OpType::SMALLER: return out << "<";
            case OpType::SMALLER_OR_EQUAL: return out << "<=";
            case OpType::GREATER: return out << ">";
            case OpType::GREATER_OR_EQUAL: return out << ">=";
        }

        return out;
    }

    std::ostream &operator<<(std::ostream &out, const Type &type) {
        switch (type.getTypeID()) {
            case Type::TypeID::INT: return out << "int";
            case Type::TypeID::FLOAT: return out << "float";
            case Type::TypeID::BOOL: return out << "bool";
            case Type::TypeID::STRING: return out << "string";
            case Type::TypeID::ARRAY: return out << *type.getSubtype() << "[]";
            case Type::TypeID::VOID: return out << "void";
            case Type::TypeID::CLASS: return out << "class " << type.getClassName();
            case Type::TypeID::AUTO: return out << "auto";
            case Type::TypeID::SELF: return out << "self";
        }

        return out;
    }

    bool Type::operator==(const Type &other) const {
        if (id == Type::TypeID::ARRAY && other.id == Type::TypeID::ARRAY) {
            return *subtype == *other.subtype;
        }

        return id == other.id && className == other.className;
    }

    bool Type::operator!=(const Type &other) const {
        return !(*this == other);
    }

    std::ostream &operator<<(std::ostream &out, AccessModifier accessModifier) {
        switch (accessModifier) {
            case AccessModifier::PUBLIC: return out << "public";
            case AccessModifier::PROTECTED: return out << "protected";
            case AccessModifier::PRIVATE: return out << "private";
        }

        return out;
    }

    bool ArgNode::operator==(const ArgNode &other) const {
        return type == other.type && name == other.name;
    }

    bool ArgNode::operator!=(const ArgNode &other) const {
        return !(*this == other);
    }

    bool FnDeclNode::operator==(const FnDeclNode &other) const {
        if (name != other.name || returnType != other.returnType) {
            return false;
        }

        if (args.size() != other.args.size()) {
            return false;
        }

        for (auto i = 0; i < args.size(); i++) {
            if (*args[i] != *other.args[i]) {
                return false;
            }
        }

        return true;
    }

    bool FnDeclNode::operator!=(const FnDeclNode &other) const {
        return !(*this == other);
    }

    bool FnDefNode::operator==(const FnDefNode &other) const {
        if (name != other.name || returnType != other.returnType) {
            return false;
        }

        if (args.size() != other.args.size()) {
            return false;
        }

        for (auto i = 0; i < args.size(); i++) {
            if (*args[i] != *other.args[i]) {
                return false;
            }
        }

        return true;
    }

    bool FnDefNode::operator!=(const FnDefNode &other) const {
        return !(*this == other);
    }

    bool MethodDeclNode::operator==(const MethodDeclNode &other) const {
        return *fnDecl == *other.fnDecl && accessModifier == other.accessModifier && isStatic == other.isStatic;
    }

    bool MethodDeclNode::operator!=(const MethodDeclNode &other) const {
        return !(*this == other);
    }

    bool MethodDefNode::operator==(const MethodDefNode &other) const {
        return *fnDef == *other.fnDef && accessModifier == other.accessModifier && isStatic == other.isStatic;
    }

    bool MethodDefNode::operator!=(const MethodDefNode &other) const {
        return !(*this == other);
    }

    bool MethodDefNode::operator==(const MethodDeclNode &decl) const {
        if (decl.getAccessModifier() != getAccessModifier()) {
            return false;
        }

        if (decl.getIsStatic() != getIsStatic()) {
            return false;
        }

        auto fnDecl = decl.getFnDecl();

        if (fnDecl->getReturnType() != fnDef->getReturnType()) {
            return false;
        }

        if (fnDecl->getArgs().size() != fnDef->getArgs().size()) {
            return false;
        }

        for (auto i = 0; i < fnDecl->getArgs().size(); i++) {
            if (*fnDecl->getArgs()[i] != *fnDef->getArgs()[i]) {
                return false;
            }
        }

        return true;
    }

    bool MethodDefNode::operator!=(const MethodDeclNode &decl) const {
        return !(*this == decl);
    }

    bool StatementListNode::isLastNodeTerminate() const {
        for (auto it = children.crbegin(); it != children.crend(); it++) {
            if (llvm::isa<CommentNode>(*it)) {
                continue;
            }

            return (*it)->isTerminate();
        }

        return false;
    }

    ClassNode::ClassNode(std::string name, StatementListNode *body, std::string parent, std::vector<std::string> interfaces, bool abstract) :
            Node(NodeKind::Class), name(std::move(name)), body(body), parent(std::move(parent)), interfaces(std::move(interfaces)), abstract(abstract) {
        for (auto child: body->getChildren()) {
            if (auto propDeclNode = llvm::dyn_cast<PropDeclNode>(child)) {
                props.push_back(propDeclNode);
            } else if (auto methodDefNode = llvm::dyn_cast<MethodDefNode>(child)) {
                auto methodName = methodDefNode->getFnDef()->getName();
                auto [_, inserted] = methods.insert({methodName, methodDefNode});
                if (!inserted) {
                    throw MethodAlreadyDeclaredException(this->name, methodName);
                }
            } else if (auto methodDeclNode = llvm::dyn_cast<MethodDeclNode>(child)) {
                if (methodDeclNode->getIsAbstract()) {
                    auto methodName = methodDeclNode->getFnDecl()->getName();
                    auto [_, inserted] = abstractMethods.insert({methodName, methodDeclNode});
                    if (!inserted) {
                        throw MethodAlreadyDeclaredException(this->name, methodName);
                    }
                }
            }
        }
    }

    InterfaceNode::InterfaceNode(std::string name, std::vector<std::string> parents, StatementListNode *body) :
            Node(NodeKind::Interface), name(std::move(name)), parents(std::move(parents)), body(body) {
        for (auto child: body->getChildren()) {
            if (auto methodDeclNode = llvm::dyn_cast<MethodDeclNode>(child)) {
                auto methodName = methodDeclNode->getFnDecl()->getName();
                auto [_, inserted] = methods.insert({methodName, methodDeclNode});
                if (!inserted) {
                    throw MethodAlreadyDeclaredException(this->name, methodName);
                }
            }
        }
    }

    void ScalarNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<ScalarNode>(this, level); }
    void StatementListNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<StatementListNode>(this, level); }
    void UnaryNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<UnaryNode>(this, level); }
    void BinaryNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<BinaryNode>(this, level); }
    void DeclNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<DeclNode>(this, level); }
    void AssignNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<AssignNode>(this, level); }
    void VarNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<VarNode>(this, level); }
    void IfNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<IfNode>(this, level); }
    void WhileNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<WhileNode>(this, level); }
    void ForNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<ForNode>(this, level); }
    void RangeNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<RangeNode>(this, level); }
    void ArgNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<ArgNode>(this, level); }
    void FnDeclNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<FnDeclNode>(this, level); }
    void FnDefNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<FnDefNode>(this, level); }
    void FnCallNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<FnCallNode>(this, level); }
    void ReturnNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<ReturnNode>(this, level); }
    void PrintlnNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<PrintlnNode>(this, level); }
    void BreakNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<BreakNode>(this, level); }
    void ContinueNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<ContinueNode>(this, level); }
    void CommentNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<CommentNode>(this, level); }
    void ClassNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<ClassNode>(this, level); }
    void PropDeclNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<PropDeclNode>(this, level); }
    void MethodDefNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<MethodDefNode>(this, level); }
    void FetchPropNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<FetchPropNode>(this, level); }
    void FetchStaticPropNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<FetchStaticPropNode>(this, level); }
    void MethodCallNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<MethodCallNode>(this, level); }
    void StaticMethodCallNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<StaticMethodCallNode>(this, level); }
    void AssignPropNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<AssignPropNode>(this, level); }
    void AssignStaticPropNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<AssignStaticPropNode>(this, level); }
    void NewNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<NewNode>(this, level); }
    void MethodDeclNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<MethodDeclNode>(this, level); }
    void InterfaceNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<InterfaceNode>(this, level); }
    void FetchArrNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<FetchArrNode>(this, level); }
    void AssignArrNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<AssignArrNode>(this, level); }
    void AppendArrNode::print(Pipes::PrintAst &astPrinter, int level) { astPrinter.print<AppendArrNode>(this, level); }

    llvm::Value *ScalarNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *StatementListNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *UnaryNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *BinaryNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *DeclNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *AssignNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *VarNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *IfNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *WhileNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ForNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *RangeNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *BreakNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ContinueNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ArgNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FnDeclNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FnDefNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FnCallNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ReturnNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *PrintlnNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *CommentNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *ClassNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *PropDeclNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *MethodDefNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FetchPropNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FetchStaticPropNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *MethodCallNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *StaticMethodCallNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *AssignPropNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *AssignStaticPropNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *NewNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *MethodDeclNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *InterfaceNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *FetchArrNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *AssignArrNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }
    llvm::Value *AppendArrNode::gen(Codegen::Codegen &codegen) { return codegen.gen(this); }

    Type ScalarNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type StatementListNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type UnaryNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type BinaryNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type DeclNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type AssignNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type VarNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type IfNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type WhileNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type ForNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type RangeNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type BreakNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type ContinueNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type ArgNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type FnDeclNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type FnDefNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type FnCallNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type ReturnNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type PrintlnNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type CommentNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type ClassNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type PropDeclNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type MethodDefNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type FetchPropNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type FetchStaticPropNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type MethodCallNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type StaticMethodCallNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type AssignPropNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type AssignStaticPropNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type NewNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type MethodDeclNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type InterfaceNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type FetchArrNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type AssignArrNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
    Type AppendArrNode::infer(Pipes::TypeInferrer &typeInferrer) { return typeInferrer.infer(this); }
}
