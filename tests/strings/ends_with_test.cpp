#include "string_test.h"

class EndsWithTest : public StringTest {
};

TEST_P(EndsWithTest, index) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
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
