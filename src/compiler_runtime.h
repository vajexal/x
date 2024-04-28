#pragma once

#include <unordered_map>
#include <string>
#include <unordered_set>

#include "type.h"

namespace X {
    class MethodDeclNode;

    struct CompilerRuntime {
        // class name -> {method name}
        std::unordered_map<std::string, std::unordered_set<std::string>> virtualMethods;

        // interface name -> {method name -> method decl}
        std::unordered_map<std::string, std::unordered_map<std::string, MethodDeclNode *>> interfaceMethods;

        // class or interface name -> implemented (or extended) interfaces
        std::unordered_map<std::string, std::unordered_set<std::string>> implementedInterfaces;

        // class name -> all extended classes
        std::unordered_map<std::string, std::unordered_set<std::string>> extendedClasses;

        // fn name -> fn type
        std::unordered_map<std::string, FnType> fnTypes;

        // class name -> {method name -> return type}
        std::unordered_map<std::string, std::unordered_map<std::string, MethodType>> classMethodTypes;
    };
}
