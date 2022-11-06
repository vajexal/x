#include "compiler_test_helper.h"

class StringIndexTest : public CompilerTest {
};

TEST_P(StringIndexTest, index) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, StringIndexTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.index(""))
)code",
                "0"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.index("hello"))
)code",
                "-1"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.index(""))
)code",
                "0"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.index("hello"))
)code",
                "0"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.index("world"))
)code",
                "6")
));
