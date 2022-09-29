#include "casts.h"

namespace X::Runtime {
    String *castBoolToString(bool value) {
        return value ? String_create("true") : String_create("false");
    }

    String *castIntToString(int64_t value) {
        auto s = std::to_string(value);
        return String_create(s.c_str());
    }

    String *castFloatToString(float value) {
        auto s = std::to_string(value);
        return String_create(s.c_str());
    }
}
