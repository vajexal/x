#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class WhileTest : public CompilerTest {
};

TEST_F(WhileTest, loop) {
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

TEST_F(WhileTest, deadCode) {
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
