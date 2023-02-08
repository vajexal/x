#include "compiler.h"

#include "compiler_runtime.h"
#include "pipeline.h"
#include "pipes/parse_code.h"
#include "pipes/print_ast.h"
#include "pipes/check_interfaces.h"
#include "pipes/check_abstract_classes.h"
#include "pipes/check_virtual_methods.h"
#include "pipes/type_inferrer.h"
#include "pipes/code_generator.h"

namespace X {
    int Compiler::compile(const std::string &code, const std::string &sourceName) {
        CompilerRuntime compilerRuntime;

        (Pipeline{})
                .pipe(new Pipes::ParseCode(code))
//                .pipe(new Pipes::PrintAst())
                .pipe(new Pipes::CheckInterfaces(compilerRuntime))
                .pipe(new Pipes::CheckAbstractClasses())
                .pipe(new Pipes::CheckVirtualMethods(compilerRuntime))
                .pipe(new Pipes::TypeInferrer(compilerRuntime))
                .pipe(new Pipes::CodeGenerator(compilerRuntime, sourceName));

        return 0;
    }
}
