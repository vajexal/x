#include "casts.h"

#include <sstream>

namespace X::Runtime {
    String *castBoolToString(bool value) {
        return value ? String_create("true") : String_create("false");
    }

    String *castIntToString(int64_t value) {
        auto s = std::to_string(value);
        return String_create(s.c_str());
    }

    String *castFloatToString(double value) {
        // we use ostringstream here because std::to_string may yield unexpected results (for example trailing zeros)
        std::ostringstream os;
        os << value;
        return String_create(os.str().c_str());
    }
}
