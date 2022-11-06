#include "compiler_test_helper.h"

class StringEndsWithTest : public CompilerTest {
};

TEST_P(StringEndsWithTest, endsWith) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringEndsWithTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.endsWith(""))
)code",
                "true"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.endsWith("hello"))
)code",
                "false"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.endsWith(""))
)code",
                "true"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.endsWith("hello world"))
)code",
                "false"),
        std::make_pair(
                R"code(
    string s = "hello world!"
    println(s.endsWith("hello"))
)code",
                "false"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.endsWith("world"))
)code",
                "true")
));
