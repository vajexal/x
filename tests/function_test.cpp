#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class FunctionTest : public CompilerTest {
};

TEST_F(FunctionTest, fnAlreadyDeclared) {
    try {
        compiler.compile(R"code(
fn a() void {}

fn a() void {}

fn main() void {}
)code");
        FAIL() << "expected FnAlreadyDeclaredException";
    } catch (const Codegen::FnAlreadyDeclaredException &e) {
        ASSERT_STREQ(e.what(), "function a already declared");
    }
}

TEST_F(FunctionTest, mainSignature) {
    try {
        compiler.compile(R"code(
fn main(int argc, []string argv) int {}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::CodegenException &e) {
        ASSERT_STREQ(e.what(), "main must take no arguments");
    }
}

TEST_F(FunctionTest, mainSignature2) {
    try {
        compiler.compile(R"code(
fn main() int {}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::CodegenException &e) {
        ASSERT_STREQ(e.what(), "main must return void");
    }
}

TEST_F(FunctionTest, declOrder) {
    checkProgram(R"code(
fn main() void {
    foo()
}

fn foo() void {
    println("foo")
}
)code", "foo");
}
