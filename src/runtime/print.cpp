#include "print.h"

#include <iostream>

#include "string.h"
#include "utils.h"

namespace X::Runtime {
    void print(Type::TypeID typeId, ...) {
        va_list args;
        va_start(args, typeId);

        switch (typeId) {
            case Type::TypeID::INT:
                std::cout << va_arg(args, int64_t);
                break;
            case Type::TypeID::FLOAT:
                std::cout << va_arg(args, double);
                break;
            case Type::TypeID::BOOL:
                std::cout << (va_arg(args, int) ? "true" : "false");
                break;
            case Type::TypeID::STRING:
                std::cout << va_arg(args, String *)->str;
                break;
            case Type::TypeID::ARRAY: {
                Type::TypeID subtypeId = va_arg(args, Type::TypeID);

                switch (subtypeId) {
                    case Type::TypeID::INT:
                        printArray(subtypeId, va_arg(args, Array<int64_t> *));
                        break;
                    case Type::TypeID::FLOAT:
                        printArray(subtypeId, va_arg(args, Array<double> *));
                        break;
                    case Type::TypeID::BOOL:
                        printArray(subtypeId, va_arg(args, Array<int> *));
                        break;
                    case Type::TypeID::STRING:
                        printArray(subtypeId, va_arg(args, Array<String *> *));
                        break;
                    default:
                        die("can't print value");
                }

                break;
            }
            default:
                die("can't print value");
        }

        va_end(args);
    }

    template<typename T>
    void printArray(Type::TypeID subtypeId, Array<T> *arr) {
        std::cout << '[';

        for (auto i = 0; i < arr->len; i++) {
            print(subtypeId, arr->data[i]);

            if (i + 1 != arr->len) {
                std::cout << ", ";
            }
        }

        std::cout << ']';
    }

    void printNewline() {
        std::cout << std::endl;
    }
}
