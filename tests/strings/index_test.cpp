#include "string_test.h"

class IndexTest : public StringResultCodeTest {
};

TEST_P(IndexTest, index) {
    auto [code, expectedResultCode] = GetParam();
    compileAndTestResultCode(code, expectedResultCode);
}

INSTANTIATE_TEST_SUITE_P(Code, IndexTest, testing::Values(
        std::make_pair(
                R"code(
    string s = ""
    return s.index("")
)code",
                0),
        std::make_pair(
                R"code(
    string s = ""
    return s.index("hello")
)code",
                -1),
        std::make_pair(
                R"code(
    string s = "hello world"
    return s.index("")
)code",
                0),
        std::make_pair(
                R"code(
    string s = "hello world"
    return s.index("hello")
)code",
                0),
        std::make_pair(
                R"code(
    string s = "hello world"
    return s.index("world")
)code",
                6)
));
