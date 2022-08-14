#include <iostream>
#include <fstream>
#include <sstream>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/TargetSelect.h"

#include "driver.h"
#include "parser.tab.hh"
#include "codegen.h"
#include "ast_printer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "missing input file\n";
    }

    std::ifstream fin(argv[1]);
    if (!fin) {
        std::cerr << "couldn't open the file\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string code = buffer.str();

    X::Driver driver(code);
    yy::parser parser(driver);
    if (parser()) {
        return 1;
    }

    llvm::LLVMContext context;
    llvm::IRBuilder<> builder(context);
    auto module = std::make_unique<llvm::Module>("Module", context);
    X::Codegen codegen(context, builder, *module.get());
    X::AstPrinter astPrinter;

    driver.result->print(astPrinter);

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    codegen.gen(driver.result);

    llvm::verifyModule(*module.get(), &llvm::errs());
    module->print(llvm::outs(), nullptr);

    auto func = module->getFunction("main");

    std::string errStr;
    llvm::ExecutionEngine *EE = llvm::EngineBuilder(std::move(module))
            .setErrorStr(&errStr)
            .create();

    if (!errStr.empty()) {
        std::cerr << errStr << std::endl;
        return 1;
    }

    auto fn = reinterpret_cast<int (*)()>(EE->getFunctionAddress(func->getName().str()));

    std::cout << "Result: " << fn() << std::endl;

    return 0;
}
