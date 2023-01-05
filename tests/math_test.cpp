#include "compiler_test_helper.h"

class MathTest : public CompilerTest {
};

TEST_F(MathTest, pow) {
    checkCode(R"code(
    println(0 ** 0)
    println((-2) ** 0)
    println(2 ** 3)
    println((-3) ** 2)
    println(2 ** 3 ** 2)
    println((2 ** 3) ** 2)
    println((-2) ** -2)
    println(4.0 ** 2)
    println(4.0 ** -0.5)
)code", R"output(1
1
8
9
512
64
0.25
16
0.5)output");
}

TEST_F(MathTest, mod) {
    checkCode(R"code(
    println(0 % 2)
    println(5 % 2)
    println(-4 % 3)
    println(3.14 % 1.2)
    println(-3.14 % 2)
)code", R"output(0
1
-1
0.74
-1.14)output");
}
