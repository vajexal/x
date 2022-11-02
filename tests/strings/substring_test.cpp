#include "compiler_test_helper.h"

class SubstringTest : public CompilerTest {
};

TEST_P(SubstringTest, substring) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, SubstringTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.substring(0, 0))
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(-2, 0))
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(0, -2))
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(0, 123))
)code",
                "abcdef\n"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(2, 123))
)code",
                "cdef\n"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(0, 6))
)code",
                "abcdef\n"),
        std::make_pair(
                R"code(
    string s = "abcdef"
    println(s.substring(1, 3))
)code",
                "bcd\n")
));
