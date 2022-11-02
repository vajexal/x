#include "compiler_test_helper.h"

class LengthTest : public CompilerTest {
};

TEST_P(LengthTest, length) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, LengthTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.length())
)code",
                "0\n"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.length())
)code",
                "5\n")
));
