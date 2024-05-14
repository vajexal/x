#include "compiler_test_helper.h"

class ArrayTest : public CompilerTest {
};

TEST_F(ArrayTest, general) {
    auto code = R"code(
    []int a = [1, 2, 3]

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
    []Foo a = [new Foo(123)]

    println(a[0].val)
}
)code";
    checkProgram(code, "123");
}

TEST_F(ArrayTest, allElementsHaveSameType) {
    try {
        compiler.compile(R"code(
fn main() void {
    auto a = [1, "foo", false]
}
)code");
    } catch (const std::exception &e) {
        ASSERT_STREQ(e.what(), "all array elements must be the same type");
    }
}
