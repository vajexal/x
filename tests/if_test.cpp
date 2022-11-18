#include "compiler_test_helper.h"

class IfTest : public CompilerTest {
};

TEST_P(IfTest, branching) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, IfTest, testing::Values(
        std::make_pair(
                R"code(
    if true {
        println("true")
    }
)code",
                "true"),
        std::make_pair(
                R"code(
    if false {
        println("true")
    } else {
        println("false")
    }
)code",
                "false"),
        std::make_pair(
                R"code(
    int i = 3
    if i == 1 {
        println(1)
    } else if i == 2 {
        println(2)
    } else if i == 3 {
        println(3)
    } else {
        println(10)
    }
)code",
                "3"),
        std::make_pair(
                R"code(
    int i = 8
    if i == 1 {
        println(1)
    } else if i == 2 {
        println(2)
    } else if i == 3 {
        println(3)
    } else {
        println(10)
    }
)code",
                "10")
));
