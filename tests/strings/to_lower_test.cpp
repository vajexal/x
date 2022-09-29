#include "compiler_test_helper.h"

class ToLowerTest : public CompilerTest {
};

TEST_P(ToLowerTest, toLower) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ToLowerTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.toLower())
)code",
                "\n"),
        std::make_pair(
                R"code(
    string s = "HELLO WORLD!"
    println(s.toLower())
)code",
                "hello world!\n")
));
