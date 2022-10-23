#include "compiler_test_helper.h"

void CompilerTest::compileAndTestOutput(const std::string &code, const std::string &expectedOutput) {
    testing::internal::CaptureStdout(); // todo customize compiler output stream

    compiler.compile(R"code(
fn main() int {
    )code" + code + R"code(
    return 0
}
)code");

    ASSERT_EQ(testing::internal::GetCapturedStdout(), expectedOutput);
}
