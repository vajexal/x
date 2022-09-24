#include "string_test.h"

class ContainsTest : public StringResultCodeTest {
};

TEST_P(ContainsTest, contains) {
    auto [code, expectedResultCode] = GetParam();
    compileAndTestResultCode(code, expectedResultCode);
}

INSTANTIATE_TEST_SUITE_P(Code, ContainsTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    if s.contains("") {
        return 1
    }
    return 0
)code",
                1),
        std::make_pair(
                R"code(
    string s = ""
    if s.contains("hello") {
        return 1
    }
    return 0
)code",
                0),
        std::make_pair(
                R"code(
    string s = "hello world"
    if s.contains("") {
        return 1
    }
    return 0
)code",
                1),
        std::make_pair(
                R"code(
    string s = "hello world"
    if s.contains("hello") {
        return 1
    }
    return 0
)code",
                1),
        std::make_pair(
                R"code(
    string s = "hello world"
    if s.contains("world") {
        return 1
    }
    return 0
)code",
                1)
));
