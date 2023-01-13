#include "code_generator.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"

#include "codegen/codegen.h"
#include "runtime/runtime.h"

namespace X::Pipes {
    StatementListNode *CodeGenerator::handle(StatementListNode *node) {
        auto context = std::make_unique<llvm::LLVMContext>();
        context->setOpaquePointers(false); // todo migrate to opaque pointers
        llvm::IRBuilder<> builder(*context);
        auto module = std::make_unique<llvm::Module>(sourceName, *context);
        Codegen::Codegen codegen(*context, builder, *module, compilerRuntime);
        Runtime::Runtime runtime{};

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

        runtime.addDeclarations(*context, builder, *module);

        codegen.gen(node);

//        module->print(llvm::outs(), nullptr);

        std::string buf;
        llvm::raw_string_ostream os(buf);
        if (llvm::verifyModule(*module, &os)) {
            throw CodeGeneratorException(os.str());
        }

        auto jitter = throwOnError(llvm::orc::LLJITBuilder().create());
        jitter->getIRTransformLayer().setTransform(OptimizationTransform());
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

    OptimizationTransform::OptimizationTransform() : PM(std::make_unique<llvm::legacy::PassManager>()) {
        llvm::PassManagerBuilder Builder;
        Builder.OptLevel = OPT_LEVEL;
        Builder.SizeLevel = SIZE_OPT_LEVEL;
        Builder.populateModulePassManager(*PM);
        Builder.Inliner = llvm::createFunctionInliningPass(OPT_LEVEL, SIZE_OPT_LEVEL, false);
    }

    llvm::Expected<llvm::orc::ThreadSafeModule> OptimizationTransform::operator()(
            llvm::orc::ThreadSafeModule TSM, llvm::orc::MaterializationResponsibility &R) {
        TSM.withModuleDo([this](llvm::Module &module) {
            PM->run(module);
        });
        return std::move(TSM);
    }
}
