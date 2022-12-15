#ifndef X_COMPILER_H
#define X_COMPILER_H

#include <string>

namespace X {
    class Compiler {
    public:
        int compile(const std::string &code, const std::string &sourceName = "narnia");
    };
}

#endif //X_COMPILER_H
