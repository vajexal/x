#pragma once

#include <vector>
#include <map>
#include <string>
#include <set>

namespace X {
    class MethodDeclNode;

    // pair<offset, class id>
    using PointerList = std::vector<std::pair<unsigned long, unsigned long>>;

    struct CompilerRuntime {
        std::map<std::string, std::set<std::string>> virtualMethods;

        // interface name -> {method name -> method decl}
        std::map<std::string, std::map<std::string, MethodDeclNode *>> interfaceMethods;

        // class or interface name -> implemented (or extended) interfaces
        std::map<std::string, std::set<std::string>> implementedInterfaces;

        // class name -> all extended classes
        std::map<std::string, std::set<std::string>> extendedClasses;

        // class id -> [{offset, class id}]
        std::map<unsigned long, PointerList> classPointerLists;
    };
}
