#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class ClassTest : public CompilerTest {
};

TEST_F(ClassTest, construct) {
    compileProgramAndTestOutput(R"code(
class Foo {
    fn construct(int a) void {
        println(a)
    }
}

fn main() int {
    Foo foo = new Foo(10)

    return 0
}
)code", "10\n");
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
    } catch (const Codegen::CodegenException &e) {
        ASSERT_STREQ(e.what(), "method not found: construct");
    }
}
