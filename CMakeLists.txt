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
        src/codegen.cpp
        src/ast_printer.cpp
        src/runtime.cpp
        src/compiler.cpp)

set(X_LIBS LLVM fmt::fmt)

add_executable(x main.cpp ${X_SOURCES})

target_link_libraries(x ${X_LIBS})

enable_testing()

add_executable(x_test ${X_SOURCES}
        tests/strings/string_test.cpp
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
        tests/strings/substring_test.cpp)
target_link_libraries(x_test GTest::gtest_main ${X_LIBS})

include(GoogleTest)
gtest_discover_tests(x_test)
