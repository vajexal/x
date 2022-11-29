#include "compiler_test_helper.h"

#include "pipes/check_abstract_classes.h"

class CheckAbstractClassesTest : public CompilerTest {
};

TEST_P(CheckAbstractClassesTest, abstract_classes) {
    auto [code, exceptionMessage] = GetParam();

    try {
        compiler.compile(code + R"code(
fn main() void {
}
)code");
        FAIL() << "expected CheckAbstractClassesException";
    } catch (const Pipes::CheckAbstractClassesException &e) {
        ASSERT_EQ(e.what(), exceptionMessage);
    }
}

INSTANTIATE_TEST_SUITE_P(Code, CheckAbstractClassesTest, testing::Values(
        std::make_pair(
                R"code(
class Foo {
    abstract public fn a() void
}
)code",
                "class Foo must be declared abstract"),
        std::make_pair(
                R"code(
abstract class Foo {
}

abstract class Foo {
}
)code",
                "class Foo already exists"),
        std::make_pair(
                R"code(
abstract class Foo {
    abstract public fn a() void
}

class Bar extends Foo {
}
)code",
                "abstract method Foo::a must be implemented"),
        std::make_pair(
                R"code(
abstract class Foo {
    abstract public fn a(int b) void
}

class Bar extends Foo {
    public fn a(float b) void {
        println(b)
    }
}
)code",
                "declaration of Bar::a must be compatible with abstract class Foo"),
        std::make_pair(
                R"code(
abstract class Foo {
    abstract public fn a() void
}

abstract class Bar extends Foo {
    abstract public fn b() void
}

class Baz extends Bar {
    public fn b() void {
        println("b")
    }
}
)code",
                "abstract method Bar::a must be implemented")
));
