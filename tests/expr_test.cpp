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

TEST_F(ExprTest, multilineChainCall) {
    checkProgram(R"code(
class Foo {
    public fn a() Foo {
        println("a")

        return this
    }

    public fn b() Foo {
        println("b")

        return this
    }

    public fn c() Foo {
        println("c")

        return this
    }
}

fn main() void {
    auto foo = new Foo()

    foo.
        a().
        b().
        c()
}
)code", R"output(a
b
c)output");
}
