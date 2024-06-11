#include "compiler_test_helper.h"

#include "codegen/codegen.h"
#include "pipes/type_inferrer.h"

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
    println([1, 2, 3])
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

TEST_F(StatementTest, consts) {
    checkProgram(R"code(
const auto x = 10

fn main() void {
    const int y = 20

    println(x + y)
}
)code", "30");
}

TEST_P(StatementTest, modifyConst) {
    auto [code, exceptionMessage] = GetParam();

    try {
        compiler.compile(code);
        FAIL() << "expected ModifyConstException";
    } catch (const Pipes::ModifyConstException &e) {
        ASSERT_EQ(e.what(), exceptionMessage);
    }
}

INSTANTIATE_TEST_SUITE_P(Code, StatementTest, testing::Values(
        std::make_pair(
                R"code(
const auto x = 10

fn main() void {
    x = 20
}
)code", "can't modify const"),
        std::make_pair(
                R"code(
fn main() void {
    const int x = 10
    x = 20
}
)code", "can't modify const"),
        std::make_pair(
                R"code(
fn main() void {
    const auto a = [1, 2, 3]
    a[] = 4
}
)code", "can't modify const"),
        std::make_pair(
                R"code(
fn main() void {
    const auto a = [1, 2, 3]
    a[1] = 4
}
)code", "can't modify const")
));
