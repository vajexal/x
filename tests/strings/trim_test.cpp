#include "compiler_test_helper.h"

class StringTrimTest : public CompilerTest {
};

TEST_P(StringTrimTest, trim) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

// todo test other spaces
INSTANTIATE_TEST_SUITE_P(Code, StringTrimTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.trim())
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "   "
    println(s.trim())
)code",
                ""),
        std::make_pair(
                R"code(
    string s = "   hello   "
    println(s.trim())
)code",
                "hello")
));
