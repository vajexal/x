#include "compiler_test_helper.h"

class StringToUpperTest : public CompilerTest {
};

TEST_P(StringToUpperTest, toUpper) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringToUpperTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.toUpper())
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "hello world!"
    println(s.toUpper())
)code",
                "HELLO WORLD!")
));
