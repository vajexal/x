#include "compiler_test_helper.h"

class ArrayTest : public CompilerTest {
};

TEST_F(ArrayTest, general) {
    auto code = R"code(
    int[] a = int[]{1, 2, 3}

    a[1] = 10

    println(a[1])
)code";
    checkCode(code, "10\n");
}
