#pragma once

#include <ostream>
#include <optional>
#include <string>
#include <vector>

namespace X {
    enum class OpType {
        PRE_INC,
        PRE_DEC,
        POST_INC,
        POST_DEC,
        OR,
        AND,
        PLUS,
        MINUS,
        MUL,
        DIV,
        NOT,
        POW,
        MOD,
        EQUAL,
        NOT_EQUAL,
        SMALLER,
        SMALLER_OR_EQUAL,
        GREATER,
        GREATER_OR_EQUAL
    };

    std::ostream &operator<<(std::ostream &out, OpType type);

    class Type {
    public:
        enum class TypeID {
            INT,
            FLOAT,
            BOOL,
            STRING,
            ARRAY,
            VOID,
            CLASS,
            AUTO,
            SELF
        };

    private:
        TypeID id;
        std::optional<std::string> className;
        Type *subtype = nullptr;

        Type(TypeID id) : id(id) {}

    public:
        Type() : id(TypeID::VOID) {} // need empty constructor for bison variant
        Type(const Type &type);
        Type(Type &&type);
        ~Type();

        Type &operator=(const Type &type);

        static Type scalar(TypeID typeID);
        static Type klass(std::string className);
        static Type array(Type &&subtype);
        static Type voidTy();
        static Type autoTy();
        static Type selfTy();

        TypeID getTypeID() const { return id; }
        const std::string &getClassName() const { return className.value(); }
        Type *getSubtype() const { return subtype; }

        bool is(TypeID typeId) const { return id == typeId; }

        bool isOneOf(TypeID typeId) const {
            return id == typeId;
        }

        template<typename... Args>
        bool isOneOf(TypeID typeID, Args... ids) const {
            return id == typeID || isOneOf(ids...);
        }

        bool operator==(const Type &other) const;
        bool operator!=(const Type &other) const;
    };

    std::ostream &operator<<(std::ostream &out, const Type &type);

    struct FnType {
        std::vector<Type> args;
        Type retType;
    };

    struct MethodType : FnType {
        bool isStatic;
    };

    struct PropType {
        Type type;
        bool isStatic;
    };
}
