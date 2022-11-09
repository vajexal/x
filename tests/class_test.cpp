#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class ClassTest : public CompilerTest {
};

TEST_F(ClassTest, construct) {
    checkProgram(R"code(
class Foo {
    fn construct(int a) void {
        println(a)
    }
}

fn main() int {
    Foo foo = new Foo(10)

    return 0
}
)code", "10");
}

TEST_F(ClassTest, constructorInheritance) {
    checkProgram(R"code(
class Foo {
    fn construct(int a) void {
        println(a)
    }
}

class Bar extends Foo {
}

fn main() int {
    Bar bar = new Bar(10)

    return 0
}
)code", "10");
}

TEST_F(ClassTest, constructorArgsMismatch) {
    try {
        compiler.compile(R"code(
class Foo {
}

fn main() int {
    Foo foo = new Foo(10)

    return 0
}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::CodegenException &e) {
        ASSERT_STREQ(e.what(), "constructor args mismatch");
    }
}

TEST_F(ClassTest, constructorIsNotCallable) {
    try {
        compiler.compile(R"code(
class Foo {
    fn construct() void {
    }
}

fn main() int {
    Foo foo = new Foo()

    foo.construct()

    return 0
}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::MethodNotFoundException &e) {
        ASSERT_STREQ(e.what(), "method not found: construct");
    }
}

TEST_F(ClassTest, inheritance) {
    try {
        compiler.compile(R"code(
class Bar extends Foo {
}

fn main() int {
    return 0
}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::CodegenException &e) {
        ASSERT_STREQ(e.what(), "class Foo not found");
    }
}

TEST_F(ClassTest, propAccess) {
    checkProgram(R"code(
class Foo {
    int val

    public fn isValPositive() bool {
        return val > 0
    }
}

fn main() int {
    Foo foo = new Foo()

    foo.val = 123
    println(foo.val)
    println(foo.isValPositive())

    return 0
}
)code", R"output(123
true)output");
}

TEST_F(ClassTest, staticPropAccess) {
    checkProgram(R"code(
class Foo {
    static int val

    public static fn isValPositive() bool {
        return val > 0
    }
}

fn main() int {
    Foo::val = 123
    println(Foo::val)
    println(Foo::isValPositive())

    return 0
}
)code", R"output(123
true)output");
}
