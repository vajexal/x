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
    auto a = []int{}
    for val in a {
        println(val)
    }
)code",
                ""),
        std::make_pair(
                R"code(
    auto a = []int{1, 2, 3}
    for val in a {
        println(val)
    }
)code", R"output(1
2
3)output"),
        std::make_pair(
                R"code(
    auto a = []int{1, 2, 3}
    for val in a {
        val++
        println(val)
    }
)code", R"output(2
3
4)output"),
        std::make_pair(
                R"code(
    auto a = []int{1, 2, 3, 4, 5}
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
4)output"),
        std::make_pair(
                R"code(
    auto a = []float{1.23, 3.14, 6.28}
    for i, val in a {
        println(i)
    }
)code", R"output(0
1
2)output"),
        std::make_pair(
                R"code(
    auto a = []float{1.23, 3.14, 6.28}
    for i, val in a {
        i++
        println(val)
    }
)code", R"output(1.23
3.14
6.28)output"),
        std::make_pair(
                R"code(
    for i in range(3) {
        println(i)
    }
)code", R"output(0
1
2)output"),
        std::make_pair(
                R"code(
    for i in range(1, 5) {
        println(i)
    }
)code", R"output(1
2
3
4)output"),
        std::make_pair(
                R"code(
    for i in range(0) {
        println(i)
    }
)code", ""),
        std::make_pair(
                R"code(
    for i in range(-3) {
        println(i)
    }
)code", ""),
        std::make_pair(
                R"code(
    for i in range(1, 0) {
        println(i)
    }
)code", ""),
        std::make_pair(
                R"code(
    for i in range(1, 10, 3) {
        println(i)
    }
)code", R"output(1
4
7)output"),
        std::make_pair(
                R"code(
    for i in range(5, -5, -3) {
        println(i)
    }
)code", R"output(5
2
-1
-4)output"),
        std::make_pair(
                R"code(
    for i in range(0, 30, 5) {
        println(i)
    }
)code", R"output(0
5
10
15
20
25)output")
));

TEST_F(ForTest, useValVarOutsideOfFor) {
    try {
        compiler.compile(R"code(
fn main() void {
    for val in []int{} {
    }

    println(val)
}
)code");
        FAIL() << "expected exception";
    } catch (const std::exception &e) {
        ASSERT_STREQ(e.what(), "var val not found");
    }
}

TEST_F(ForTest, forScope) {
    checkCode(R"code(
    auto i = 10

    println(i)

    for i in range(3) {
        println(i)
    }

    println(i)
)code", R"output(10
0
1
2
10)output");
}
