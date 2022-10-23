#include "compiler.h"

#include "pipeline.h"
#include "pipes/parse_code.h"
#include "pipes/print_ast.h"
#include "pipes/check_interfaces.h"
#include "pipes/check_abstract_classes.h"
#include "pipes/code_generator.h"

namespace X {
    int Compiler::compile(const std::string &code) {
        Pipeline pipeline{};
        auto node = pipeline
                .pipe(new Pipes::ParseCode(code))
//                .pipe(new Pipes::PrintAst())
                .pipe(new Pipes::CheckInterfaces())
                .pipe(new Pipes::CheckAbstractClasses())
                .pipe(new Pipes::CodeGenerator())
                .get();

        delete node;

        return 0;
    }
}
