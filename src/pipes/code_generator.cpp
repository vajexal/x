#include "code_generator.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Passes/PassBuilder.h"

#include "codegen/codegen.h"
#include "runtime/runtime.h"
#include "gc/pass.h"

namespace X::Pipes {
    TopStatementListNode *CodeGenerator::handle(TopStatementListNode *node) {
        auto context = std::make_unique<llvm::LLVMContext>();
        llvm::IRBuilder<> builder(*context);
        auto module = std::make_unique<llvm::Module>(sourceName, *context);
        GC::GC gc;
        Codegen::Codegen codegen(*context, builder, *module, compilerRuntime, gc);
        Runtime::Runtime runtime{};

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

        runtime.addDeclarations(*context, builder, *module);

        codegen.genProgram(node);

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

        // link gc
        auto runtimeGCSymbol = throwOnError(jitter->lookup(Mangler::mangleInternalSymbol("gc")));
        auto runtimeGCPtr = runtimeGCSymbol.toPtr<GC::GC **>();
        *runtimeGCPtr = &gc; // nolint

        auto mainFn = throwOnError(jitter->lookup(Codegen::Codegen::MAIN_FN_NAME));
        auto *fn = mainFn.toPtr<void()>();

        fn();

        // we can't run gc in alloc for now because we don't have intermediate roots ("h(f(), g())"),
        gc.run();

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

    OptimizationTransform::OptimizationTransform() {}

    llvm::Expected<llvm::orc::ThreadSafeModule> OptimizationTransform::operator()(
            llvm::orc::ThreadSafeModule TSM, llvm::orc::MaterializationResponsibility &R) {
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        llvm::PassBuilder PB;

        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        PB.registerPipelineStartEPCallback([&](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level) {
            MPM.addPass(llvm::createModuleToFunctionPassAdaptor(GC::XGCLowering()));
        });

        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

        TSM.withModuleDo([&](llvm::Module &module) {
            MPM.run(module, MAM);
        });

        return std::move(TSM);
    }
}
