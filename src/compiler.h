#pragma once

#include <string>

namespace X {
    class Compiler {
    public:
        int compile(const std::string &code, const std::string &sourceName = "narnia");
    };
}
