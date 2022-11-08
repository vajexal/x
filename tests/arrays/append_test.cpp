#include "compiler_test_helper.h"

#include "runtime/runtime.h"

class ArrayAppendTest : public CompilerTest {
};

TEST_P(ArrayAppendTest, append) {
    ASSERT_EQ(Runtime::Array::MIN_CAP, 8);
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ArrayAppendTest, testing::Values(
        std::make_pair(
                R"code(
    []int a = []int{}
    a[] = 1
    println(a.length())
    println(a[0])
)code",
                "1\n1"),
        std::make_pair(
                R"code(
    []int a = []int{1, 2, 3, 4, 5, 6, 7, 8}
    a[] = 9
    println(a.length())
    println(a[8])
)code",
                "9\n9")
));
