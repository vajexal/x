#include "compiler_test_helper.h"

class ArrayLengthTest : public CompilerTest {
};

TEST_P(ArrayLengthTest, length) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ArrayLengthTest, testing::Values(
        std::make_pair(
                R"code(
    int[] a = int[]{}
    println(a.length())
)code",
                "0"),
        std::make_pair(
                R"code(
    int[] a = int[]{1, 2, 3}
    println(a.length())
)code",
                "3")
));
