#include <iostream>
#include <fstream>
#include <sstream>

#include "compiler.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "missing input file" << std::endl;
        return 1;
    }

    std::string filename = argv[1];

    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "couldn't open the file" << std::endl;
        return 1;
    }
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string code = buffer.str();

    X::Compiler compiler;

    compiler.compile(code, filename);

    return 0;
}
