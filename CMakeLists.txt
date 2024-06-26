cmake_minimum_required(VERSION 3.22)
project(X)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(LLVM_ENABLE_ASSERTIONS ON)

find_package(LLVM REQUIRED CONFIG)

FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 9.1.0)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1)
FetchContent_MakeAvailable(googletest)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

include_directories(.)
include_directories(./src)
include_directories(./tests)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(X_SOURCES lexer.cpp
        parser.tab.cc
        src/ast.cpp
        src/type.cpp
        src/utils.cpp
        src/codegen/codegen.cpp
        src/codegen/expr.cpp
        src/codegen/statement.cpp
        src/codegen/function.cpp
        src/codegen/class.cpp
        src/codegen/decl.cpp
        src/codegen/gc.cpp
        src/runtime/runtime.cpp
        src/runtime/string.cpp
        src/runtime/array.cpp
        src/runtime/print.cpp
        src/compiler.cpp
        src/gc/gc.cpp
        src/gc/strategy.cpp
        src/gc/pass.cpp
        src/pipes/parse_code.cpp
        src/pipes/print_ast.cpp
        src/pipes/check_interfaces.cpp
        src/pipes/check_abstract_classes.cpp
        src/pipes/check_virtual_methods.cpp
        src/pipes/type_inferrer.cpp
        src/pipes/const_string_folding.cpp
        src/pipes/code_generator.cpp)

set(X_LIBS LLVM fmt::fmt)

add_custom_command(
        OUTPUT lexer.cpp
        COMMAND re2c ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l -o lexer.cpp -ci -W
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
)

add_custom_command(
        OUTPUT parser.tab.cc parser.tab.hh location.hh
        COMMAND bison -d ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y -W
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
)

add_executable(x src/main.cpp ${X_SOURCES})

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
        tests/arrays/append_test.cpp
        tests/statement_test.cpp
        tests/expr_test.cpp
        tests/if_test.cpp
        tests/while_test.cpp
        tests/for_test.cpp
        tests/operators_test.cpp
        tests/casts_test.cpp
        tests/function_test.cpp
        tests/class_test.cpp
        tests/interface_test.cpp
        tests/abstract_class_test.cpp
        tests/type_inferrer_test.cpp
        tests/tour_test.cpp
        tests/math_test.cpp)
target_link_libraries(x_test GTest::gtest_main ${X_LIBS})

include(GoogleTest)
gtest_discover_tests(x_test)
