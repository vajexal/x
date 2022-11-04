cmake_minimum_required(VERSION 3.22)
project(X)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(LLVM_ENABLE_ASSERTIONS ON)

find_package(LLVM REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)
find_package(GTest CONFIG REQUIRED)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

include_directories(.)
include_directories(./src)
include_directories(./tests)

set(X_SOURCES lexer.cpp
        parser.tab.cc
        src/ast.cpp
        src/utils.cpp
        src/codegen/codegen.cpp
        src/codegen/expr.cpp
        src/codegen/function.cpp
        src/codegen/class.cpp
        src/runtime/runtime.cpp
        src/runtime/string.cpp
        src/runtime/array.cpp
        src/runtime/casts.cpp
        src/compiler.cpp
        src/pipes/parse_code.cpp
        src/pipes/print_ast.cpp
        src/pipes/code_generator.cpp
        src/pipes/check_interfaces.cpp
        src/pipes/check_abstract_classes.cpp)

set(X_LIBS LLVM fmt::fmt)

add_executable(x main.cpp ${X_SOURCES})

target_link_libraries(x ${X_LIBS})

enable_testing()

add_executable(x_test ${X_SOURCES}
        tests/compiler_test_helper.cpp
        tests/strings/concat_test.cpp
        tests/strings/length_test.cpp
        tests/strings/is_empty_test.cpp
        tests/strings/trim_test.cpp
        tests/strings/to_lower_test.cpp
        tests/strings/to_upper_test.cpp
        tests/strings/index_test.cpp
        tests/strings/contains_test.cpp
        tests/strings/starts_with_test.cpp
        tests/strings/ends_with_test.cpp
        tests/strings/substring_test.cpp
        tests/arrays/array_test.cpp
        tests/arrays/length_test.cpp
        tests/arrays/is_empty_test.cpp
        tests/operators_test.cpp
        tests/check_interfaces_test.cpp
        tests/check_abstract_classes_test.cpp
        tests/class_test.cpp)
target_link_libraries(x_test GTest::gtest_main ${X_LIBS})

include(GoogleTest)
gtest_discover_tests(x_test)
