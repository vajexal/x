#include "compiler_test_helper.h"

class StringToLowerTest : public CompilerTest {
};

TEST_P(StringToLowerTest, toLower) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringToLowerTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.toLower())
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "HELLO WORLD!"
    println(s.toLower())
)code",
                "hello world!")
));
