#include "code_generator.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Support/TargetSelect.h"

#include "codegen/codegen.h"
#include "runtime/runtime.h"

namespace X::Pipes {
    StatementListNode *CodeGenerator::handle(StatementListNode *node) {
        auto context = std::make_unique<llvm::LLVMContext>();
        context->setOpaquePointers(false); // todo migrate to opaque pointers
        llvm::IRBuilder<> builder(*context);
        auto module = std::make_unique<llvm::Module>("Module", *context);
        Codegen::Codegen codegen(*context, builder, *module, compilerRuntime);
        Runtime::Runtime runtime{};

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

        runtime.addDeclarations(*context, builder, *module);

        codegen.gen(node);

        llvm::verifyModule(*module, &llvm::errs());
//        module->print(llvm::outs(), nullptr);

        auto jitter = throwOnError(llvm::orc::LLJITBuilder().create());
        throwOnError(jitter->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));

        llvm::orc::MangleAndInterner llvmMangle(jitter->getExecutionSession(), jitter->getDataLayout());
        runtime.addDefinitions(jitter->getMainJITDylib(), llvmMangle);

        auto mainFn = throwOnError(jitter->lookup("main"));
        auto *fn = mainFn.toPtr<void()>();

        fn();

        return node;
    }

    void CodeGenerator::throwOnError(llvm::Error &&err) const {
        if (err) {
            throw CodeGeneratorException(llvm::toString(std::move(err)));
        }
    }

    template<typename T>
    T CodeGenerator::throwOnError(llvm::Expected<T> &&val) const {
        if (auto err = val.takeError()) {
            throw CodeGeneratorException(llvm::toString(std::move(err)));
        }
        return std::move(*val);
    }
}

