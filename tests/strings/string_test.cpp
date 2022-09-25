#include "string_test.h"

void StringTest::compileAndTestOutput(const std::string &code, const std::string &expectedOutput) {
    testing::internal::CaptureStdout(); // todo customize compiler output stream

    compiler.compile(R"code(
fn main() int {
    )code" + code + R"code(
    return 0
}
)code");

    ASSERT_EQ(testing::internal::GetCapturedStdout(), expectedOutput);
}
