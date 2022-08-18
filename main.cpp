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
#include "runtime.h"
#include "ast_printer.h"

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

    X::Driver driver(code);
    yy::parser parser(driver);
    if (parser()) {
        return 1;
    }

    llvm::LLVMContext context;
    llvm::IRBuilder<> builder(context);
    auto module = std::make_unique<llvm::Module>("Module", context);
    X::Codegen codegen(context, builder, *module.get());
    X::Runtime runtime;
    X::AstPrinter astPrinter;

    driver.result->print(astPrinter);

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    runtime.addDeclarations(context, *module.get());

    codegen.gen(driver.result);

    llvm::verifyModule(*module.get(), &llvm::errs());
    module->print(llvm::outs(), nullptr);

    std::string errStr;
    llvm::ExecutionEngine *engine = llvm::EngineBuilder(std::move(module))
            .setErrorStr(&errStr)
            .create();

    if (!errStr.empty()) {
        std::cerr << errStr << std::endl;
        return 1;
    }

    engine->DisableSymbolSearching();

    runtime.addGlobalMappings(*engine);

    auto mainFn = engine->FindFunctionNamed("main");
    if (!mainFn) {
        std::cerr << "main function not found" << std::endl;
        return 1;
    }
    auto fn = reinterpret_cast<int (*)()>(engine->getFunctionAddress(mainFn->getName().str()));

    return fn();
}
