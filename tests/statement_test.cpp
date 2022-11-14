#include "compiler_test_helper.h"

class StatementTest : public CompilerTest {
};

TEST_F(StatementTest, decl) {
    auto code = R"code(
    int i
    float f
    bool b
    string s
    []int a

    println(i)
    println(f)
    println(b)
    println(s)
    println(a.isEmpty())
)code";

    checkCode(code, R"output(0
0
false

true)output");
}
