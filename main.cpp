#include <iostream>
#include <fstream>
#include <sstream>

#include "compiler.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "missing input file" << std::endl;
        return 1;
    }

    std::ifstream fin(argv[1]);
    if (!fin) {
        std::cerr << "couldn't open the file" << std::endl;
        return 1;
    }
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string code = buffer.str();

    X::Compiler compiler;

    return compiler.compile(code);
}
