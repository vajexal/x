#ifndef X_COMPILER_RUNTIME_H
#define X_COMPILER_RUNTIME_H

#include <map>
#include <string>
#include <set>

namespace X {
    class MethodDeclNode;

    struct CompilerRuntime {
        std::map<std::string, std::set<std::string>> virtualMethods;

        // interface name -> {method name -> method decl}
        std::map<std::string, std::map<std::string, MethodDeclNode *>> interfaceMethods;

        // class or interface name -> implemented (or extended) interfaces
        std::map<std::string, std::set<std::string>> implementedInterfaces;

        // class name -> all extended classes
        std::map<std::string, std::set<std::string>> extendedClasses;
    };
}

#endif //X_COMPILER_RUNTIME_H
