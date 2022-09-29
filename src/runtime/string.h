#ifndef X_STRING_H
#define X_STRING_H

#include <string>

namespace X::Runtime {
    struct String {
        static inline const std::string CLASS_NAME = "String";
        static inline const std::string CONSTRUCTOR_FN_NAME = "String_construct";

        char *str;
        uint64_t len;
    };

    String *String_new(uint64_t len = 0);
    void String_construct(String *that, const char *s);
    String *String_create(const char *s);
    String *String_copy(String *str);
    String *String_concat(String *that, String *other);
    uint64_t String_length(String *that);
    bool String_isEmpty(String *that);
    String *String_trim(String *that);
    String *String_toLower(String *that);
    String *String_toUpper(String *that);
    int64_t String_index(String *that, String *other);
    bool String_contains(String *that, String *other);
    bool String_startsWith(String *that, String *other);
    bool String_endsWith(String *that, String *other);
    String *String_substring(String *that, int64_t offset, int64_t length);
}

#endif //X_STRING_H
