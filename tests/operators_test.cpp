#include "compiler_test_helper.h"

class OperatorsTest : public CompilerTest {
};

TEST_P(OperatorsTest, operators) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, OperatorsTest, testing::Values(
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
    if "" {
        println("yes")
    } else {
        println("no")
    }
)code",
                "no"),
        std::make_pair(
                R"code(
    if []int{1, 2, 3} {
        println("yes")
    } else {
        println("no")
    }
)code",
                "yes"),
        std::make_pair(
                R"code(
    println(2 && 3.14)
    println(0 && 0.0)
    println(3.14 && false)
    println(1 && 2 && 3)
)code",
                R"output(true
false
false
true)output")
));

TEST_P(OperatorsTest, logicalAndLazyEval) {
    checkProgram(R"code(
fn fnWithSideEffect() int {
    println("hello")
    return 0
}

fn main() void {
    println(fnWithSideEffect() && fnWithSideEffect())
}
)code", R"output(hello
false)output");
}
