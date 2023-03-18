#pragma once

#include "string.h"

namespace X::Runtime {
    String *castBoolToString(bool value);
    String *castIntToString(int64_t value);
    String *castFloatToString(double value);
}
