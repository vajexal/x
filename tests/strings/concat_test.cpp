#include <gtest/gtest.h>

#include "compiler.h"

class ConcatTest : public testing::TestWithParam<std::pair<std::string, std::string>> {
protected:
    X::Compiler compiler;
};

TEST_P(ConcatTest, concat) {
    testing::internal::CaptureStdout(); // todo customize compiler output stream

    auto [code, expectedOutput] = GetParam();
    code = R"code(
fn main() int {
    )code" + code + R"code(
    return 0
}
)code";

    compiler.compile(code);

    ASSERT_EQ(testing::internal::GetCapturedStdout(), expectedOutput);
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
