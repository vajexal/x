#include "string_test.h"

class LengthTest : public StringResultCodeTest {
};

TEST_P(LengthTest, concat) {
    auto [code, expectedResultCode] = GetParam();
    compileAndTestResultCode(code, expectedResultCode);
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
