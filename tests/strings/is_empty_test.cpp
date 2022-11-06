#include "compiler_test_helper.h"

class StringIsEmptyTest : public CompilerTest {
};

TEST_P(StringIsEmptyTest, isEmpty) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringIsEmptyTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.isEmpty())
)code",
                "true\n"),
        std::make_pair(
                R"code(
    string s = "   "
    println(s.isEmpty())
)code",
                "false\n"),
        std::make_pair(
                R"code(
    string s = "0"
    println(s.isEmpty())
)code",
                "false\n")
));
