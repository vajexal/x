#include "compiler_test_helper.h"

class ArrayTest : public CompilerTest {
};

TEST_F(ArrayTest, general) {
    auto code = R"code(
    []int a = []int{1, 2, 3}

    a[1] = 10

    println(a[1])
)code";
    checkCode(code, "10");
}

TEST_F(ArrayTest, arbitraryArrayType) {
    auto code = R"code(
class Foo {
    public int val

    public fn construct(int v) void {
        val = v
    }
}

fn main() void {
    []Foo a = []Foo{new Foo(123)}

    println(a[0].val)
}
)code";
    checkProgram(code, "123");
}
