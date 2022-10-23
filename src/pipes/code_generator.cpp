#include "code_generator.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/TargetSelect.h"

#include "codegen/codegen.h"
#include "runtime/runtime.h"

namespace X::Pipes {
    StatementListNode *CodeGenerator::handle(StatementListNode *node) {
        llvm::LLVMContext context;
        context.setOpaquePointers(false); // todo migrate to opaque pointers
        llvm::IRBuilder<> builder(context);
        llvm::Module module("Module", context);
        Codegen::Codegen codegen(context, builder, module);
        Runtime::Runtime runtime;

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

        runtime.addDeclarations(context, module);

        codegen.gen(node);

        llvm::verifyModule(module, &llvm::errs());
//        module->print(llvm::outs(), nullptr);

        std::string errStr;
        llvm::ExecutionEngine *engine = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(&module))
                .setErrorStr(&errStr)
                .create();

        if (!errStr.empty()) {
            throw CodeGeneratorException(errStr);
        }

        engine->DisableSymbolSearching();

        runtime.addDefinitions(*engine);

        auto mainFn = engine->FindFunctionNamed("main");
        if (!mainFn) {
            throw CodeGeneratorException("main function not found");
        }
        auto fn = reinterpret_cast<int (*)()>(engine->getFunctionAddress(mainFn->getName().str()));

        fn();

        return node;
    }
}

