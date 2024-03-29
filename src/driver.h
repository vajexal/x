#pragma once

#include "ast.h"
#include "location.hh"

namespace X {
    class Driver {
    public:
        TopStatementListNode *root = nullptr;
        const char *s;
        yy::location location;

        Driver(const char *s) : s(s) {}
        Driver(const std::string &s) : s(s.c_str()) {}
    };
}
