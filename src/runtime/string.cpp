#include "string.h"

#include "utils.h"

namespace X::Runtime {
    String *String_new(uint64_t len) {
        auto res = new String;
        res->len = len;
        res->str = new char[res->len + 1];
        res->str[res->len] = 0;
        return res;
    }

    void String_construct(String *that, const char *s) {
        that->len = std::strlen(s);
        that->str = new char[that->len + 1];
        that->str[that->len] = 0;
        std::memcpy(that->str, s, that->len);
    }

    String *String_create(const char *s) {
        auto res = new String;
        String_construct(res, s);
        return res;
    }

    String *String_copy(String *str) {
        auto res = String_new(str->len);
        std::memcpy(res->str, str->str, str->len);
        return res;
    }

    String *String_concat(String *that, String *other) {
        if (!other->len) {
            return String_copy(that);
        }

        auto res = String_new(that->len + other->len);
        std::memcpy(res->str, that->str, that->len);
        std::memcpy(res->str + that->len, other->str, other->len);
        return res;
    }

    uint64_t String_length(String *that) {
        return that->len;
    }

    bool String_isEmpty(String *that) {
        return !String_length(that);
    }

    String *String_trim(String *that) {
        if (!that->len) {
            return String_new();
        }

        auto startIdx = 0;
        for (; startIdx < that->len && isspace(that->str[startIdx]); startIdx++) {}

        if (startIdx == that->len - 1) {
            return String_new();
        }

        auto endIdx = that->len - 1;
        for (; endIdx > startIdx && isspace(that->str[endIdx]); endIdx--) {}

        auto res = String_new(endIdx - startIdx + 1);
        std::memcpy(res->str, that->str + startIdx, res->len);
        return res;
    }

    String *String_toLower(String *that) {
        auto res = String_new(that->len);

        for (auto i = 0; i < that->len; i++) {
            res->str[i] = (char)std::tolower(that->str[i]);
        }

        return res;
    }

    String *String_toUpper(String *that) {
        auto res = String_new(that->len);

        for (auto i = 0; i < that->len; i++) {
            res->str[i] = (char)std::toupper(that->str[i]);
        }

        return res;
    }

    int64_t String_index(String *that, String *other) {
        auto res = std::strstr(that->str, other->str);
        return res ? res - that->str : -1;
    }

    bool String_contains(String *that, String *other) {
        return String_index(that, other) != -1;
    }

    bool String_startsWith(String *that, String *other) {
        if (other->len > that->len) {
            return false;
        }

        return std::strncmp(that->str, other->str, other->len) == 0;
    }

    bool String_endsWith(String *that, String *other) {
        if (other->len > that->len) {
            return false;
        }

        return std::strncmp(that->str + that->len - other->len, other->str, other->len) == 0;
    }

    String *String_substring(String *that, int64_t offset, int64_t length) {
        if (offset < 0 || length <= 0 || offset > that->len) {
            return String_new();
        }

        if (length + offset > that->len) {
            length = (int64_t)that->len - offset;
        }

        auto res = String_new(length);
        std::memcpy(res->str, that->str + offset, length);
        return res;
    }

    String *createEmptyString() {
        return String_new();
    }
}
