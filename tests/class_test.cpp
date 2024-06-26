#include "compiler_test_helper.h"

#include "codegen/codegen.h"

class ClassTest : public CompilerTest {
};

TEST_F(ClassTest, construct) {
    checkProgram(R"code(
class Foo {
    fn construct(int a) void {
        println(a)
    }
}

fn main() void {
    Foo foo = new Foo(10)
}
)code", "10");
}

TEST_F(ClassTest, constructorInheritance) {
    checkProgram(R"code(
class Foo {
    fn construct(int a) void {
        println(a)
    }
}

class Bar extends Foo {}

fn main() void {
    Bar bar = new Bar(10)
}
)code", "10");
}

TEST_F(ClassTest, constructorIsNotCallable) {
    try {
        compiler.compile(R"code(
class Foo {
    fn construct() void {
    }
}

fn main() void {
    Foo foo = new Foo()

    foo.construct()
}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::MethodNotFoundException &e) {
        ASSERT_STREQ(e.what(), "method Foo::construct not found");
    }
}

TEST_F(ClassTest, inheritance) {
    try {
        compiler.compile(R"code(
class Bar extends Foo {}

fn main() void {}
)code");
        FAIL() << "expected exception";
    } catch (const std::exception &e) {
        ASSERT_STREQ(e.what(), "class Foo not found");
    }
}

TEST_F(ClassTest, propAccess) {
    checkProgram(R"code(
class Foo {
    int val

    public fn isValPositive() bool {
        return val > 0
    }
}

fn main() void {
    Foo foo = new Foo()

    foo.val = 123
    println(foo.val)
    println(foo.isValPositive())
}
)code", R"output(123
true)output");
}

TEST_F(ClassTest, staticPropAccess) {
    checkProgram(R"code(
class Foo {
    static int val

    public static fn isValPositive() bool {
        return val > 0
    }
}

fn main() void {
    Foo::val = 123
    println(Foo::val)
    println(Foo::isValPositive())
}
)code", R"output(123
true)output");
}

TEST_F(ClassTest, allocateObjectsOnHeap) {
    checkProgram(R"code(
class Greeter {
    []string names

    fn construct([]string _names) void {
        names = _names
    }

    public fn greet() void {
        for name in names {
            println("hey " + name)
        }
    }
}

fn foo() Greeter {
    return new Greeter(["Ron", "Billy"])
}

fn main() void {
    Greeter greeter = foo()

    greeter.greet()
}
)code", R"output(hey Ron
hey Billy)output");
}

TEST_F(ClassTest, polymorphism) {
    checkProgram(R"code(
class Foo {
    public fn a() void {
        println("foo")
    }
}

class Bar extends Foo {
    public fn a() void {
        println("bar")
    }
}

fn check(Foo foo) void {
    foo.a()
}

fn main() void {
    Bar bar = new Bar()

    check(bar)
}
)code", "bar");
}

TEST_F(ClassTest, polymorphism2) {
    checkProgram(R"code(
class Foo {
    public fn a() void {
        println("foo")
    }
}

class Bar extends Foo {
    public fn a() void {
        println("bar")
    }
}

class Baz extends Bar {}

fn check(Foo foo) void {
    foo.a()
}

fn main() void {
    Baz baz = new Baz()

    check(baz)
}
)code", "bar");
}

TEST_F(ClassTest, interfacePolymorphism) {
    checkProgram(R"code(
interface A {
    public fn a() void
}

interface B extends A {
    public fn b() void
}

class C implements B {
    public fn a() void {
        println("ca")
    }

    public fn b() void {
        println("cb")
    }
}

class D extends C {
    public fn a() void {
        println("da")
    }
}

fn foo(A a) void {
    a.a()
}

fn main() void {
    D d = new D()

    foo(d)
}
)code", "da");
}

TEST_F(ClassTest, privatePropAccess) {
    try {
        compiler.compile(R"code(
class Foo {
    private int a
}

class Bar extends Foo {
    public fn b() void {
        println(a)
    }
}

fn main() void {
    Bar bar = new Bar()
    bar.b()
}
)code");
        FAIL() << "expected PropAccessException";
    } catch (const Codegen::PropAccessException &e) {
        ASSERT_STREQ(e.what(), "cannot access private property Foo::a");
    }
}

TEST_F(ClassTest, privateStaticPropAccess) {
    try {
        compiler.compile(R"code(
class Foo {
    private static int a
}

class Bar extends Foo {
    public static fn b() void {
        println(a)
    }
}

fn main() void {
    Bar::b()
}
)code");
        FAIL() << "expected PropAccessException";
    } catch (const Codegen::PropAccessException &e) {
        ASSERT_STREQ(e.what(), "cannot access private property Foo::a");
    }
}

TEST_F(ClassTest, privateMethodAccess) {
    try {
        compiler.compile(R"code(
class Foo {
    private fn a() void {
    }
}

class Bar extends Foo {
    public fn b() void {
        a()
    }
}

fn main() void {
    Bar bar = new Bar()
    bar.b()
}
)code");
        FAIL() << "expected MethodAccessException";
    } catch (const Codegen::MethodAccessException &e) {
        ASSERT_STREQ(e.what(), "cannot access private method Foo::a");
    }
}

TEST_F(ClassTest, privateStaticMethodAccess) {
    try {
        compiler.compile(R"code(
class Foo {
    private static fn a() void {
    }
}

class Bar extends Foo {
    public static fn b() void {
        a()
    }
}

fn main() void {
    Bar::b()
}
)code");
        FAIL() << "expected MethodAccessException";
    } catch (const Codegen::MethodAccessException &e) {
        ASSERT_STREQ(e.what(), "cannot access private method Foo::a");
    }
}

TEST_F(ClassTest, classAlreadyExists) {
    try {
        compiler.compile(R"code(
class Foo {}

class Foo {}

fn main() void {}
)code");
        FAIL() << "expected SymbolAlreadyExistsException";
    } catch (const Codegen::ClassAlreadyExistsException &e) {
        ASSERT_STREQ(e.what(), "class Foo already exists");
    }
}

TEST_F(ClassTest, methodAlreadyDeclared) {
    try {
        compiler.compile(R"code(
class Foo {
    public fn a() void {
    }

    public fn a() void {
    }
}

fn main() void {}
)code");
        FAIL() << "expected AstException";
    } catch (const AstException &e) {
        ASSERT_STREQ(e.what(), "method Foo::a already declared");
    }
}

TEST_F(ClassTest, methodAlreadyDeclared2) {
    try {
        compiler.compile(R"code(
abstract class Foo {
    abstract public fn a() void

    public fn a() void {
    }
}

fn main() void {}
)code");
        FAIL() << "expected MethodAlreadyDeclaredException";
    } catch (const Codegen::MethodAlreadyDeclaredException &e) {
        ASSERT_STREQ(e.what(), "method Foo::a already declared");
    }
}

TEST_F(ClassTest, abstractMethodAlreadyDeclared) {
    try {
        compiler.compile(R"code(
abstract class Foo {
    abstract public fn a() void
    abstract public fn a() void
}

fn main() void {}
)code");
        FAIL() << "expected AstException";
    } catch (const AstException &e) {
        ASSERT_STREQ(e.what(), "method Foo::a already declared");
    }
}

TEST_F(ClassTest, propAlreadyDeclared) {
    try {
        compiler.compile(R"code(
class Foo {
    int a
    int a
}

fn main() void {}
)code");
        FAIL() << "expected PropAlreadyDeclaredException";
    } catch (const Codegen::PropAlreadyDeclaredException &e) {
        ASSERT_STREQ(e.what(), "property Foo::a already declared");
    }
}

TEST_F(ClassTest, staticPropAlreadyDeclared) {
    try {
        compiler.compile(R"code(
class Foo {
    static int a
    static int a
}

fn main() void {}
)code");
        FAIL() << "expected PropAlreadyDeclaredException";
    } catch (const Codegen::PropAlreadyDeclaredException &e) {
        ASSERT_STREQ(e.what(), "property Foo::a already declared");
    }
}

TEST_F(ClassTest, symbolAlreadyExists) {
    try {
        compiler.compile(R"code(
interface Foo {}

class Foo {}

fn main() void {}
)code");
        FAIL() << "expected SymbolAlreadyExistsException";
    } catch (const Codegen::SymbolAlreadyExistsException &e) {
        ASSERT_STREQ(e.what(), "symbol Foo already exists");
    }
}

TEST_F(ClassTest, cannotInstatinateAbstractClass) {
    try {
        compiler.compile(R"code(
abstract class Foo {}

fn main() void {
    Foo foo = new Foo()
}
)code");
        FAIL() << "expected CodegenException";
    } catch (const Codegen::CodegenException &e) {
        ASSERT_STREQ(e.what(), "cannot instantiate abstract class Foo");
    }
}

TEST_F(ClassTest, cannotInstatinateInterface) {
    try {
        compiler.compile(R"code(
interface Foo {}

fn main() void {
    Foo foo = new Foo()
}
)code");
        FAIL() << "expected exception";
    } catch (const std::exception &e) {
        ASSERT_STREQ(e.what(), "invalid type");
    }
}

TEST_F(ClassTest, that) {
    checkProgram(R"code(
class Greeter {
    private string name

    public fn construct(string name) void {
        this.name = name
    }

    public fn greet() void {
        println("hello " + this.name)
    }
}

fn main() void {
    auto greeter = new Greeter("Peter")

    greeter.greet()
}

)code", "hello Peter");
}

TEST_F(ClassTest, cannotAssignThis) {
    try {
        compiler.compile(R"code(
class Foo {
    public fn a() void {
        this = this
    }
}
)code");
        FAIL() << "expected exception";
    } catch (const std::exception &e) {}
}

TEST_F(ClassTest, self) {
    checkProgram(R"code(
class Foo {
    public static int a

    public static fn b() void {
        self::a = 1

        println(self::a)
    }
}

fn main() void {
    Foo::b()
}
)code", "1");
}

TEST_F(ClassTest, cannotAssignSelf) {
    try {
        compiler.compile(R"code(
class Foo {
    public static fn a() void {
        self = self
    }
}
)code");
        FAIL() << "expected exception";
    } catch (const std::exception &e) {}
}

TEST_F(ClassTest, methodDeclOrder) {
    checkProgram(R"code(
class Foo {
    public fn a() void {
        println("a")

        b()
    }

    public fn b() void {
        println("b")
    }
}

fn main() void {
    auto foo = new Foo()

    foo.a()
}
)code", R"output(a
b)output");
}

TEST_F(ClassTest, classDeclOrder) {
    checkProgram(R"code(
fn main() void {
    auto foo = new Foo()

    foo.bar.a()
}

class Foo {
    public Bar bar

    fn construct() void {
        bar = new Bar()
    }
}

class Bar {
    public fn a() void {
        println("a")
    }
}
)code", "a");
}

TEST_F(ClassTest, staticWithInitializer) {
    checkProgram(R"code(
class Foo {
    public static int a = 1
}

class Bar {
    public static auto b = Foo::a + 2
}

fn main() void {
    println(Bar::b)
}
)code", "3");
}

TEST_F(ClassTest, propWithInitializer) {
    checkProgram(R"code(
class Foo {
    public int a = 1
}

class Bar extends Foo {
    public int b = 2
}

fn main() void {
    auto bar = new Bar()

    println(bar.a + bar.b)
}
)code", "3");
}

TEST_F(ClassTest, parentWithInit) {
    checkProgram(R"code(
class Foo {
    public int a = 1
}

class Bar extends Foo {}

fn main() void {
    auto bar = new Bar()

    println(bar.a)
}
)code", "1");
}
