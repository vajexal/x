#include "compiler_test_helper.h"

#include "pipes/type_inferrer.h"

class TypeInferrerTest : public CompilerTest {
};

TEST_P(TypeInferrerTest, inferring) {
    auto [code, exceptionMessage] = GetParam();

    try {
        compiler.compile(code);
        FAIL() << "expected TypeInferrerException";
    } catch (const Pipes::TypeInferrerException &e) {
        ASSERT_EQ(e.what(), exceptionMessage);
    }
}

INSTANTIATE_TEST_SUITE_P(Code, TypeInferrerTest, testing::Values(
        // invalid types
        std::make_pair(
                R"code(
fn main() void {
    void x
}
)code",
                "invalid type"),
        std::make_pair(
                R"code(
fn main() void {
    []void x
}
)code",
                "invalid type"),
        std::make_pair(
                R"code(
fn foo() void {}

fn main() void {
    auto x = foo()
}
)code",
                "invalid type"),
        // check decl and assignment
        std::make_pair(
                R"code(
fn main() void {
    int a = "hello"
}
)code",
                "invalid type"),
        // fn return type
        std::make_pair(
                R"code(
fn foo() int {
    return 123
}

fn main() void {
    string s = foo()
}
)code",
                "invalid type"),
        // fn args
        std::make_pair(
                R"code(
fn foo(int a) void {
}

fn main() void {
    foo("hello")
}
)code",
                "invalid type"),
        // call args mismatch
        std::make_pair(
                R"code(
fn foo(int a) void {
}

fn main() void {
    foo()
}
)code",
                "call args mismatch"),
        // can't declare void arg
        std::make_pair(
                R"code(
fn foo(void a) void {
}

fn main() void {
}
)code",
                "invalid type"),
        std::make_pair(
                R"code(
fn foo([]void a) void {
}

fn main() void {
}
)code",
                "invalid type"),
        // can't cast condition to bool
        std::make_pair(
                R"code(
class Foo {}

fn main() void {
    Foo foo = new Foo()
    if foo {
        println("here we go")
    }
}
)code",
                "invalid type"),
        std::make_pair(
                R"code(
class Foo {}

fn main() void {
    Foo foo = new Foo()
    while foo {
        println("here we go")
    }
}
)code",
                "invalid type"),
        // unary and binary expressions
        std::make_pair(
                R"code(
fn main() void {
    bool b = "hello" > 123
}
)code",
                "invalid type"),
        std::make_pair(
                R"code(
fn main() void {
    bool b = "hello" * 123
}
)code",
                "invalid type"),
        std::make_pair(
                R"code(
fn main() void {
    string s = "hello"
    s++
}
)code",
                "invalid type"),
        // for expr
        std::make_pair(
                R"code(
fn main() void {
    for val in 123 {
    }
}
)code",
                "for expression must be array or range"),
        std::make_pair(
                R"code(
fn main() void {
    for val in range(3.14) {
    }
}
)code",
                "range stop argument must be int"),
        std::make_pair(
                R"code(
fn main() void {
    for val in range(false, 10) {
    }
}
)code",
                "range start argument must be int"),
        std::make_pair(
                R"code(
fn main() void {
    for val in range(0, 3, "step") {
    }
}
)code",
                "range step argument must be int"),
        // internal funcs
        std::make_pair(
                R"code(
fn main() void {
    exit(1)
}
)code",
                "call args mismatch"),
        // prop assign
        std::make_pair(
                R"code(
class Foo {
    public int a
}

fn main() void {
    Foo foo = new Foo()
    foo.a = "hello"
}
)code",
                "invalid type"),
        // static prop assign
        std::make_pair(
                R"code(
class Foo {
    public static int a
}

fn main() void {
    Foo::a = "hello"
}
)code",
                "invalid type"),
        // check method call
        std::make_pair(
                R"code(
class Foo {
    public fn foo() int {
        return 123
    }
}

class Bar extends Foo {}

fn main() void {
    Bar bar = new Bar()
    string s = bar.foo()
}
)code",
                "invalid type"),
        // check constructor call
        std::make_pair(
                R"code(
class Foo {
    fn construct(string s) void {}
}

class Bar extends Foo {}

fn main() void {
    Bar bar = new Bar(123)
}
)code",
                "invalid type"),
        // array assign
        std::make_pair(
                R"code(
fn main() void {
    []int a = [1, 2, 3]
    a[1] = "hello"
}
)code",
                "invalid type"),
        // array append
        std::make_pair(
                R"code(
fn main() void {
    []int a
    a[] = "hello"
}
)code",
                "invalid type"),
        // self return type on function
        std::make_pair(
                R"code(
fn foo() self {
}
)code",
                "invalid type"),
        // call non-static method as static
        std::make_pair(
                R"code(
class Foo {
    public fn a() void {}
}

fn main() void {
    Foo::a()
}
)code",
                "wrong method call Foo::a"),
        // call static method as non-static
        std::make_pair(
                R"code(
class Foo {
    public static fn a() void {}
}

fn main() void {
    Foo foo = new Foo()

    foo.a()
}
)code",
                "wrong method call Foo::a"),
        // fetch non-static prop as static
        std::make_pair(
                R"code(
class Foo {
    public int a
}

fn main() void {
    println(Foo::a)
}
)code",
                "wrong prop access Foo::a"),
        // fetch static prop as non-static
        std::make_pair(
                R"code(
class Foo {
    public static int a
}

fn main() void {
    Foo foo = new Foo()

    println(foo.a)
}
)code",
                "wrong prop access Foo::a")
));
