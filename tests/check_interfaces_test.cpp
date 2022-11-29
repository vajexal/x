#include "compiler_test_helper.h"

#include "pipes/check_interfaces.h"

class CheckInterfacesTest : public CompilerTest {
};

TEST_P(CheckInterfacesTest, interfaces) {
    auto [code, exceptionMessage] = GetParam();

    try {
        compiler.compile(code + R"code(
fn main() void {
}
)code");
        FAIL() << "expected CheckInterfacesException";
    } catch (const Pipes::CheckInterfacesException &e) {
        ASSERT_EQ(e.what(), exceptionMessage);
    }
}

INSTANTIATE_TEST_SUITE_P(Code, CheckInterfacesTest, testing::Values(
        std::make_pair(
                R"code(
class Bar implements Foo {
    public fn a() void {
        println("hello")
    }
}
)code",
                "interface Foo not found"),
        std::make_pair(
                R"code(
interface Foo {
}

interface Foo {
}
)code",
                "interface Foo already declared"),
        std::make_pair(
                R"code(
interface Foo {
    public fn a() void
}

class Bar implements Foo {
}
)code",
                "interface method Foo::a must be implemented"),
        std::make_pair(
                R"code(
interface Foo {
    public fn a(int b) void
}

class Bar implements Foo {
    public fn a(float b) void {
        println(b)
    }
}
)code",
                "declaration of Bar::a must be compatible with interface Foo"),
        std::make_pair(
                R"code(
interface Foo {
    public fn a(int b) void
}

class Bar implements Foo {
    private fn a(int b) void {
        println(b)
    }
}
)code",
                "declaration of Bar::a must be compatible with interface Foo"),
        std::make_pair(
                R"code(
interface Foo {
    public static fn a(int b) void
}

class Bar implements Foo {
    public fn a(int b) void {
        println(b)
    }
}
)code",
                "declaration of Bar::a must be compatible with interface Foo"),
        std::make_pair(
                R"code(
interface A {
    public fn a() void
}

interface B {
    public fn b() void
}

interface C extends A, B {
    public fn c() void
}

class D implements C {
    public fn a() void {
    }

    public fn c() void {
    }
}
)code",
                "interface method C::b must be implemented"),
        std::make_pair(
                R"code(
interface A {
    public fn a(int foo) void
}

interface B {
    public fn a(int bar) void
}

interface C extends A, B {
    public fn c() void
}
)code",
                "interface method C::a is incompatible")
));

TEST_F(CheckInterfacesTest, hiddenImplementation) {
    checkProgram(R"code(
interface A {
    public fn a() void
}

class Foo {
    public fn a() void {
        println("foo")
    }
}

class Bar extends Foo implements A {
}

fn main() void {
    Bar bar = new Bar()
    bar.a()
}
)code", "foo");
}

TEST_F(CheckInterfacesTest, methodAlreadyDeclared) {
    try {
        compiler.compile(R"code(
interface Foo {
    public fn a() void
    public fn a() void
}

fn main() void {
}
)code");
        FAIL() << "expected AstException";
    } catch (const AstException &e) {
        ASSERT_STREQ(e.what(), "method Foo::a already declared");
    }
}
