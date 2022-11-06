#include "compiler_test_helper.h"

class StringConcatTest : public CompilerTest {
};

TEST_P(StringConcatTest, concat) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringConcatTest, testing::Values(
        std::make_pair(
                R"code(
    string s = "hello"
    string s2 = " world"
    println(s.concat(s2))
)code",
                "hello world\n"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.concat("hello"))
)code",
                "hello\n"),
        std::make_pair(
                R"code(
    string s = "hello"
    println(s.concat(""))
)code",
                "hello\n")
));
