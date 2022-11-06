#include "compiler_test_helper.h"

class StringStartsWithTest : public CompilerTest {
};

TEST_P(StringStartsWithTest, startsWith) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringStartsWithTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.startsWith(""))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.startsWith("hello"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.startsWith(""))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.startsWith("hello world"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello world!"
    println(s.startsWith("world"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.startsWith("hello"))
)code",
                "true\n")
));
