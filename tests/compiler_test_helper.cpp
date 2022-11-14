#include "compiler_test_helper.h"

void CompilerTest::checkCode(const std::string &code, const std::string &expectedOutput) {
    auto program = R"code(
fn main() void {
    )code" + code + R"code(
}
)code";

    checkProgram(program, expectedOutput);
}

void CompilerTest::checkProgram(const std::string &code, const std::string &expectedOutput) {
    testing::internal::CaptureStdout(); // todo customize compiler output stream

    compiler.compile(code);

    auto output = testing::internal::GetCapturedStdout();
    output.erase(output.find_last_not_of("\n") + 1);

    ASSERT_EQ(output, expectedOutput);
}
