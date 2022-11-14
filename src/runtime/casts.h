#ifndef X_CASTS_H
#define X_CASTS_H

#include "string.h"

namespace X::Runtime {
    String *castBoolToString(bool value);
    String *castIntToString(int64_t value);
    String *castFloatToString(double value);
}

#endif //X_CASTS_H
