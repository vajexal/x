#include "compiler_test_helper.h"

class StringLengthTest : public CompilerTest {
};

TEST_P(StringLengthTest, length) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringLengthTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.length())
)code",
                "0"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.length())
)code",
                "5")
));
