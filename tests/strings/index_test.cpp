#include "string_test.h"

class IndexTest : public StringTest {
};

TEST_P(IndexTest, index) {
    auto [code, expectedOutput] = GetParam();
    compileAndTestOutput(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, IndexTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    println(s.index(""))
)code",
                "0\n"),
        std::make_pair(
                R"code(
    string s = ""
    println(s.index("hello"))
)code",
                "-1\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.index(""))
)code",
                "0\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.index("hello"))
)code",
                "0\n"),
        std::make_pair(
                R"code(
    string s = "hello world"
    println(s.index("world"))
)code",
                "6\n")
));
