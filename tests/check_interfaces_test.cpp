#include "compiler_test_helper.h"

#include "pipes/check_interfaces.h"

class CheckInterfacesTest : public CompilerTest {
};

TEST_P(CheckInterfacesTest, interfaces) {
    auto [code, exceptionMessage] = GetParam();

    try {
        compiler.compile(code + R"code(
fn main() int {
    return 0
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
                "declaration of Bar::a must be compatible with interface Foo")
));
