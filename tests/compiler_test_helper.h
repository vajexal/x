#pragma once

#include <gtest/gtest.h>

#include "compiler.h"

using namespace X;

class CompilerTest : public testing::TestWithParam<std::pair<std::string, std::string>> {
protected:
    Compiler compiler;

    void checkCode(const std::string &code, const std::string &expectedOutput);
    void checkProgram(const std::string &code, const std::string &expectedOutput);
};
