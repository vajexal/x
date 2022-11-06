#include "compiler_test_helper.h"

class StringSubstringTest : public CompilerTest {
};

TEST_P(StringSubstringTest, substring) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringSubstringTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.substring(0, 0))
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(-2, 0))
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(0, -2))
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(0, 123))
)code",
                "abcdef"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(2, 123))
)code",
                "cdef"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(0, 6))
)code",
                "abcdef"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(1, 3))
)code",
                "bcd")
));
