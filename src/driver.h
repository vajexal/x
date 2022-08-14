#ifndef X_DRIVER_H
#define X_DRIVER_H

#include "ast.h"

namespace X {
    class Driver {
    public:
        StatementListNode *result = nullptr;
        const char *s;

        Driver(const char *s) : s(s) {}
        Driver(const std::string &s): s(s.c_str()) {}
    };
}


#endif //X_DRIVER_H
