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

// todo print result and remove this class
class StringResultCodeTest : public testing::TestWithParam<std::pair<std::string, int64_t>> {
protected:
    Compiler compiler;

    void compileAndTestResultCode(const std::string &code, int64_t expectedResultCode);
};

#endif //X_STRING_TEST_H
