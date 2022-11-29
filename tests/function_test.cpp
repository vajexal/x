#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class FunctionTest : public CompilerTest {
};

TEST_F(FunctionTest, fnAlreadyDeclared) {
    try {
        compiler.compile(R"code(
fn a() void {
}

fn a() void {
}

fn main() void {
}
)code");
        FAIL() << "expected FnAlreadyDeclaredException";
    } catch (const Codegen::FnAlreadyDeclaredException &e) {
        ASSERT_STREQ(e.what(), "function a already declared");
    }
}
