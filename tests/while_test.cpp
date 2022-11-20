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
