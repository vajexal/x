#include "compiler_test_helper.h"

class StringContainsTest : public CompilerTest {
};

TEST_P(StringContainsTest, contains) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringContainsTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.contains(""))
)code",
                "true"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.contains("hello"))
)code",
                "false"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.contains(""))
)code",
                "true"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.contains("hello"))
)code",
                "true"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.contains("world"))
)code",
                "true")
));
