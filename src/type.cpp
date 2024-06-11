#include "type.h"

namespace X {
    Type::Type(const Type &type) : id(type.id), constant(type.constant) {
        if (type.className) {
            className = type.className.value();
        }

        if (type.subtype) {
            subtype = new Type(*type.subtype);
        }
    }

    Type::Type(Type &&type) : id(type.id), className(std::move(type.className)), subtype(type.subtype), constant(type.constant) {
        type.id = TypeID::VOID;
        type.className = std::nullopt;
        type.subtype = nullptr;
        type.constant = false;
    }

    Type &Type::operator=(const Type &type) {
        id = type.id;
        constant = type.constant;

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

    Type Type::scalar(Type::TypeID typeId) {
        if (typeId == TypeID::CLASS || typeId == TypeID::ARRAY || typeId == TypeID::AUTO || typeId == TypeID::SELF) {
            throw std::invalid_argument("invalid type for scalar");
        }

        Type type;
        type.id = typeId;

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
        return {TypeID::VOID};
    }

    Type Type::autoTy() {
        return {TypeID::AUTO};
    }

    Type Type::selfTy() {
        return {TypeID::SELF};
    }

    std::ostream &operator<<(std::ostream &out, OpType type) {
        switch (type) {
            case OpType::PRE_INC:
                return out << "++ ";
            case OpType::PRE_DEC:
                return out << "-- ";
            case OpType::POST_INC:
                return out << " ++";
            case OpType::POST_DEC:
                return out << " --";
            case OpType::OR:
                return out << "||";
            case OpType::AND:
                return out << "&&";
            case OpType::PLUS:
                return out << "+";
            case OpType::MINUS:
                return out << "-";
            case OpType::MUL:
                return out << "*";
            case OpType::DIV:
                return out << "/";
            case OpType::NOT:
                return out << "!";
            case OpType::POW:
                return out << "**";
            case OpType::MOD:
                return out << "%";
            case OpType::EQUAL:
                return out << "==";
            case OpType::NOT_EQUAL:
                return out << "!=";
            case OpType::SMALLER:
                return out << "<";
            case OpType::SMALLER_OR_EQUAL:
                return out << "<=";
            case OpType::GREATER:
                return out << ">";
            case OpType::GREATER_OR_EQUAL:
                return out << ">=";
        }

        return out;
    }

    std::ostream &operator<<(std::ostream &out, const Type &type) {
        if (type.isConst()) {
            out << "const ";
        }

        switch (type.getTypeID()) {
            case Type::TypeID::INT:
                return out << "int";
            case Type::TypeID::FLOAT:
                return out << "float";
            case Type::TypeID::BOOL:
                return out << "bool";
            case Type::TypeID::STRING:
                return out << "string";
            case Type::TypeID::ARRAY:
                return out << *type.getSubtype() << "[]";
            case Type::TypeID::VOID:
                return out << "void";
            case Type::TypeID::CLASS:
                return out << "class " << type.getClassName();
            case Type::TypeID::AUTO:
                return out << "auto";
            case Type::TypeID::SELF:
                return out << "self";
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
}
