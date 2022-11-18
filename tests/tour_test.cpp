#include "compiler_test_helper.h"

class TourTest : public CompilerTest {
};

TEST_F(TourTest, tour) {
    auto code = R"code(
// Single line comment

// classes will be described later
class Foo {
}

fn types() void {
    // variable declaration and assigment.
    int i // i == 0
    float f = 3.14
    bool b = false // or "true"

    // strings

    // we can declare string literal using double quotes
    // strings are byte arrays (not utf8) for now
    string s = "hello"

    // we can concat strings
    s = s + " world" // s == "hello world"

    // retrieve its length
    println(s.length()) // 11

    // check if string is empty
    println(s.isEmpty()) // false

    // explicitly concat
    s = s.concat("!") // s == "hello world!"

    // upper string
    s = s.toUpper() // s == "HELLO WORLD!"

    // lower string
    s = s.toLower() // s == "hello world"

    // check for substring.
    // If substring is not found then index will return -1
    int pos = s.index("world")
    println(pos) // 6

    // check if string contains substring
    println(s.contains("world")) // true

    // check if string starts with substring
    println(s.startsWith("hello")) // true

    // check if string ends with substring
    println(s.endsWith("world")) // false

    // retrieve substring, params are int offset, int length.
    // If offset or length is invalid then empty string is returned
    println(s.substring(0, 5)) // hello

    // arrays
    // we can declare array literal using curly braces
    []int a = []int{1, 2, 3}

    // retrieve array length
    println(a.length()) // 3

    // check if array is empty
    println(a.isEmpty()) // false

    a[1] = 10 // set element's value by index

    println(a[1]) // get element by index

    a[] = 4 // append new element
    println(a.length()) // 4

    // we can create array of arbitrary types
    []Foo arr = []Foo{new Foo(), new Foo()}
}

fn flow() void {
    // will print "then"
    if true {
        println("then")
    }

    // will print "else"
    if false {
        println("then")
    } else {
        println("else")
    }

    // will print "2"
    int i = 2
    if i == 0 {
        println(0)
    } else if i == 1 {
        println(1)
    } else if i == 2 {
        println(2)
    } else {
        println(10)
    }

    i = 0
    while true {
        i++

        if i < 5 {
            continue
        }

        if i >= 10 {
            break
        }
    }

    println(i) // 10

    // also we can use strings and arrays as conditions
    if "hello" {
        println("string is not empty")
    }

    []int a = []int{1, 2, 3}
    if a {
        println("array is not empty")
    }

    // for construction iterates over array. On each iteration the value of current element is assigned to val
    for val in a {
        // it's possible to use break and continue inside of for construction (like in while construction)
        println(val)
    }
}

// function definition. Function takes two arguments and returns float.
fn bar(int a, float b) float {
    return a * b
}

// classes and inheritance

abstract class Animal {
    private string name

    public fn construct(string animalName) void {
        name = animalName
    }

    public fn getName() string {
        return name
    }

    abstract public fn makeSound() void
}

class Cat extends Animal {
    public fn makeSound() void {
        println("meow!")
    }
}

class Dog extends Animal {
    public fn makeSound() void {
        println("bark!")
    }
}

// interfaces

interface A {
    public fn foo() void
}

interface B {
    public fn bar() void
}

// multiple interface inheritance
interface C extends A, B {
    public fn baz() void
}

class D implements C {
    public fn foo() void {
    }

    public fn bar() void {
    }

    public fn baz() void {
    }
}

// class can also implement multiple interfaces
class E implements A, B {
    public fn foo() void {
    }

    public fn bar() void {
    }
}

// class can extend and implement simultaneously
class F {
}

class G extends F implements A {
    public fn foo() void {
    }
}

// properties and methods can be static

class GlobalCounter {
    public static int value

    public static fn resetCounter() void {
        value = 0
    }
}

// entry point for the program
fn main() void {
    // println outputs a line to stdout
    println("hello world")

    types()
    flow()

    // we can call previously defined function like this
    float f = bar(2, 3.14)
    println(f) // 6.28

    // creating objects
    Cat luna = new Cat("Luna")
    println(luna.getName()) // "Luna"
    luna.makeSound()

    // accessing static properties and methods
    GlobalCounter::value = 123
    println(GlobalCounter::value) // 123
    GlobalCounter::resetCounter() // GlobalCounter::value == 0
}
)code";
    checkProgram(code, R"output(hello world
11
false
6
true
true
false
hello
3
false
10
4
then
else
2
10
string is not empty
array is not empty
1
2
3
6.28
Luna
meow!
123)output");
}
