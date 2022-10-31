#ifndef X_COMPILER_TEST_HELPER_H
#define X_COMPILER_TEST_HELPER_H

#include <gtest/gtest.h>

#include "compiler.h"

using namespace X;

class CompilerTest : public testing::TestWithParam<std::pair<std::string, std::string>> {
protected:
    Compiler compiler;

    void compileAndTestOutput(const std::string &code, const std::string &expectedOutput);
    void compileProgramAndTestOutput(const std::string &code, const std::string &expectedOutput);
};

#endif //X_COMPILER_TEST_HELPER_H
