#include "compiler_test_helper.h"

class OperatorsTest : public CompilerTest {
};

TEST_P(OperatorsTest, operators) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(StringOperators, OperatorsTest, testing::Values(
        std::make_pair(
                R"code(
    println("foo" + "bar")
)code",
                "foobar"),
        std::make_pair(
                R"code(
    println("foo" == "foo")
    println("foo" == "bar")
    println("foo" != "bar")
)code",
                R"output(true
false
true)output"),
        std::make_pair(
                R"code(
    string s = ""
    if s {
        println("yes")
    } else {
        println("no")
    }
)code",
                "no"),
        std::make_pair(
                R"code(
    int[] a = int[]{1, 2, 3}
    if a {
        println("yes")
    } else {
        println("no")
    }
)code",
                "yes")
));
