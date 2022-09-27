#include "string_test.h"

class IsEmptyTest : public StringTest {
};

TEST_P(IsEmptyTest, concat) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, IsEmptyTest, testing::Values(
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
