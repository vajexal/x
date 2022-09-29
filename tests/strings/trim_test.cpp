#include "compiler_test_helper.h"

class TrimTest : public CompilerTest {
};

TEST_P(TrimTest, trim) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
}

// todo test other spaces
INSTANTIATE_TEST_SUITE_P(Code, TrimTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.trim())
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "   "
    println(s.trim())
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "   hello   "
    println(s.trim())
)code",
                "hello\n")
));
