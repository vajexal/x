#include "compiler_test_helper.h"

void CompilerTest::compileAndTestOutput(const std::string &code, const std::string &expectedOutput) {
    auto program = R"code(
fn main() int {
    )code" + code + R"code(
    return 0
}
)code";

    compileProgramAndTestOutput(program, expectedOutput);
}

void CompilerTest::compileProgramAndTestOutput(const std::string &code, const std::string &expectedOutput) {
    testing::internal::CaptureStdout(); // todo customize compiler output stream

    compiler.compile(code);

    ASSERT_EQ(testing::internal::GetCapturedStdout(), expectedOutput);
}
