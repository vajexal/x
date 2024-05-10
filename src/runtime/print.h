#pragma once

#include "type.h"
#include "array.h"

namespace X::Runtime {
    void print(Type::TypeID typeId, ...);

    template<typename T>
    void printArray(Type::TypeID subtypeId, Array<T> *arr);

    void printNewline();
}
