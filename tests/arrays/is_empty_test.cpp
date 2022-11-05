#include "compiler_test_helper.h"

class ArrayIsEmptyTest : public CompilerTest {
};

TEST_P(ArrayIsEmptyTest, isEmpty) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ArrayIsEmptyTest, testing::Values(
        std::make_pair(
                R"code(
    int[] a = int[]{}
    println(a.isEmpty())
)code",
                "true\n"),
        std::make_pair(
                R"code(
    int[] a = int[]{1, 2, 3}
    println(a.isEmpty())
)code",
                "false\n")
));
