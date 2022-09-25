#ifndef X_STRING_TEST_H
#define X_STRING_TEST_H

#include <gtest/gtest.h>

#include "compiler.h"

using namespace X;

class StringTest : public testing::TestWithParam<std::pair<std::string, std::string>> {
protected:
    Compiler compiler;

    void compileAndTestOutput(const std::string &code, const std::string &expectedOutput);
};

#endif //X_STRING_TEST_H
