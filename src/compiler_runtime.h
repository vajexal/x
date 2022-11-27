#ifndef X_COMPILER_RUNTIME_H
#define X_COMPILER_RUNTIME_H

#include <map>
#include <string>
#include <set>

namespace X {
    struct CompilerRuntime {
        std::map<std::string, std::set<std::string>> virtualMethods;
    };
}

#endif //X_COMPILER_RUNTIME_H
