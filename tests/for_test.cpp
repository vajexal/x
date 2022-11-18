#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class ForTest : public CompilerTest {
};

TEST_P(ForTest, foreach) {
    auto [code, expectedOutput] = GetParam();
    checkCode(code, expectedOutput);
}

INSTANTIATE_TEST_SUITE_P(Code, ForTest, testing::Values(
        std::make_pair(
                R"code(
    []int a = []int{}
    for val in a {
        println(val)
    }
)code",
                ""),
        std::make_pair(
                R"code(
    []int a = []int{1, 2, 3}
    for val in a {
        println(val)
    }
)code", R"output(1
2
3)output"),
        std::make_pair(
                R"code(
    []int a = []int{1, 2, 3}
    for val in a {
        val++
        println(val)
    }
)code", R"output(2
3
4)output"),
        std::make_pair(
                R"code(
    []int a = []int{1, 2, 3, 4, 5}
    for val in a {
        if val == 2 {
            continue
        }

        println(val)

        if val == 4 {
            break
        }
    }
)code", R"output(1
3
4)output")
));

TEST_F(ForTest, usingValVarOutsideOfFor) {
    try {
        compiler.compile(R"code(
fn main() void {
    []int a = []int{}
    for val in a {
    }

    println(val)
}
)code");
        FAIL() << "expected VarNotFoundException";
    } catch (const Codegen::VarNotFoundException &e) {
        ASSERT_STREQ(e.what(), "var not found: val");
    }
}

TEST_F(ForTest, forOverPrimitive) {
    try {
        compiler.compile(R"code(
fn main() void {
    int a
    for val in a {
    }
}
)code");
        FAIL() << "expected InvalidTypeException";
    } catch (const Codegen::InvalidTypeException &e) {
        // todo excepted type
    }
}