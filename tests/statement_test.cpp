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
    println(a.isEmpty())
)code";

    checkCode(code, R"output(0
0
false

true)output");
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
