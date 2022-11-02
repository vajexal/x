#include "compiler_test_helper.h"

class EndsWithTest : public CompilerTest {
};

TEST_P(EndsWithTest, endsWith) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, EndsWithTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.endsWith(""))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.endsWith("hello"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.endsWith(""))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.endsWith("hello world"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello world!"
    println(s.endsWith("hello"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.endsWith("world"))
)code",
                "true\n")
));
