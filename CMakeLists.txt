cmake_minimum_required(VERSION 3.22)
project(X)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(LLVM_ENABLE_ASSERTIONS ON)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

include_directories(.)
include_directories(./src)

add_executable(x main.cpp lexer.cpp parser.tab.cc src/ast.cpp src/driver.cpp src/codegen.cpp src/ast_printer.cpp)

target_link_libraries(x LLVM)
