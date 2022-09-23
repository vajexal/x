#include <gtest/gtest.h>

#include "compiler.h"

class LengthTest : public testing::TestWithParam<std::pair<std::string, uint64_t>> {
protected:
    X::Compiler compiler;
};

TEST_P(LengthTest, concat) {
    auto [code, expectedResultCode] = GetParam();
    code = R"code(
fn main() int {
    )code" + code + R"code(
}
)code";

    ASSERT_EQ(compiler.compile(code), expectedResultCode);
}

INSTANTIATE_TEST_SUITE_P(Code, LengthTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    return s.length()
)code",
                0),
        std::make_pair(
                R"code(
    string s = "hello"
    return s.length()
)code",
                5)
));
