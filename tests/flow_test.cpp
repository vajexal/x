#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class FlowTest : public CompilerTest {
};

TEST_P(FlowTest, branching) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, FlowTest, testing::Values(
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

TEST_F(FlowTest, loop) {
    auto code = R"code(
    int i = 0
    while true {
        if i < 5 {
            i = i + 2
            continue
        }

        i++

        if i >= 10 {
            break
        }
    }
    println(i)
)code";

    checkCode(code, "10");
}

TEST_F(FlowTest, deadCode) {
    try {
        compiler.compile(R"code(
fn main() void {
    while true {
        break
        continue
    }
}
)code");
        FAIL() << "expected DeadCodeException";
    } catch (const Codegen::DeadCodeException &e) {}
}
