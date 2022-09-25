#include "string_test.h"

class ContainsTest : public StringTest {
};

TEST_P(ContainsTest, contains) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ContainsTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.contains(""))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.contains("hello"))
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.contains(""))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.contains("hello"))
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.contains("world"))
)code",
                "true\n")
));
