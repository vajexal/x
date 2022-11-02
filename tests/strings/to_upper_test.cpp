#include "compiler_test_helper.h"

class ToUpperTest : public CompilerTest {
};

TEST_P(ToUpperTest, toUpper) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ToUpperTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.toUpper())
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "hello world!"
    println(s.toUpper())
)code",
                "HELLO WORLD!\n")
));
