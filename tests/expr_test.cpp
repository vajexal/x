#include "compiler_test_helper.h"

class ExprTest : public CompilerTest {
};

TEST_F(ExprTest, calc) {
    auto code = R"code(
    println(2 + 2 * 2)
    println((2 + 2) * 2)
    println(!0)
    println(!0.0)
    println(!123)
    println(!1.23)
    println(!true)
    println(!true)
)code";

    checkCode(code, R"output(6
8
true
true
false
false
false
false)output");
}

TEST_F(ExprTest, methodOnExpr) {
    checkCode(R"code(
    println(("foo" + "bar").length())
)code", "6");
}
