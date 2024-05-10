#include "compiler_test_helper.h"

#include "codegen/codegen.h"

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
    println(a)
)code";

    checkCode(code, R"output(0
0
false

[])output");
}

TEST_F(StatementTest, printArray) {
    checkCode(R"code(
    println([]int{1, 2, 3})
)code", "[1, 2, 3]");
}

TEST_F(StatementTest, varAlreadyExists) {
    try {
        compiler.compile(R"code(
fn main() void {
    int a
    int a
}
)code");
        FAIL() << "expected VarAlreadyExistsException";
    } catch (const Codegen::VarAlreadyExistsException &e) {
        ASSERT_STREQ(e.what(), "var a already exists");
    }
}

TEST_F(StatementTest, globals) {
    checkProgram(R"code(
int x = 1

fn foo() void {
    x = 2
}

fn main() void {
    println(x)
    foo()
    println(x)
}
)code", R"output(1
2)output");
}

TEST_F(StatementTest, globals2) {
    checkProgram(R"code(
int foo = 1
float bar = 2
auto fooBar = foo + bar

fn main() void {
    println(fooBar)
}
)code", "3");
}
