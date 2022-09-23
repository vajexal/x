#include "string_test.h"

class ConcatTest : public StringTest {
};

TEST_P(ConcatTest, concat) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ConcatTest, testing::Values(
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
