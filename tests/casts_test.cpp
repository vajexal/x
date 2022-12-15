#include "compiler_test_helper.h"

class CastsTest : public CompilerTest {
};

TEST_F(CastsTest, varImplicitCast) {
    auto code = R"code(
    float f = 1
    f = 2
    println(f)
)code";

    checkCode(code, "2");
}

TEST_F(CastsTest, arrayElemImplicitCast) {
    auto code = R"code(
    []float a
    a[] = 1
    a[0] = 2
    println(a[0])
)code";

    checkCode(code, "2");
}

TEST_F(CastsTest, fnArgImplicitCast) {
    auto code = R"code(
class Foo {}

class Bar extends Foo {}

fn baz(Foo foo) void {}

fn main() void {
    Bar bar = new Bar()

    baz(bar)
}
)code";

    checkProgram(code, "");
}

TEST_F(CastsTest, propImplicitCast) {
    auto code = R"code(
class Foo {
    float f
}

fn main() void {
    Foo foo = new Foo()

    foo.f = 1

    println(foo.f)
}
)code";

    checkProgram(code, "1");
}

TEST_F(CastsTest, methodArgImplicitCast) {
    auto code = R"code(
class Foo {
    public fn bar(float f) void {
        println(f)
    }
}

fn main() void {
    Foo foo = new Foo()

    foo.bar(1)
}
)code";

    checkProgram(code, "1");
}

TEST_F(CastsTest, staticPropImplicitCast) {
    auto code = R"code(
class Foo {
    static float f
}

fn main() void {
    Foo::f = 1

    println(Foo::f)
}
)code";

    checkProgram(code, "1");
}

TEST_F(CastsTest, staticMethodArgImplicitCast) {
    auto code = R"code(
class Foo {
    public static fn bar(float f) void {
        println(f)
    }
}

fn main() void {
    Foo::bar(1)
}
)code";

    checkProgram(code, "1");
}

TEST_F(CastsTest, returnImplicitCast) {
    auto code = R"code(
fn foo() float {
    return 1
}

fn main() void {
    println(foo())
}
)code";

    checkProgram(code, "1");
}
