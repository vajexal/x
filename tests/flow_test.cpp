#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class FlowTest : public CompilerTest {
};

TEST_F(FlowTest, branching) {
    auto code = R"code(
    if true {
        if 0 {
            println(1)
        } else {
            println(2)
        }
    } else {
        println(3)
    }
)code";

    checkCode(code, "2");
}

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
