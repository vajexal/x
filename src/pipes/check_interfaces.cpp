#include "check_interfaces.h"

#include <fmt/core.h>

#include "utils.h"

namespace X::Pipes {
    TopStatementListNode *CheckInterfaces::handle(TopStatementListNode *node) {
        for (auto interface: node->interfaces) {
            addInterface(interface);
        }

        for (auto klass: node->classes) {
            checkClass(klass);
        }

        return node;
    }

    void CheckInterfaces::addInterface(InterfaceNode *node) {
        auto &name = node->name;

        if (compilerRuntime->interfaceMethods.contains(name)) {
            throw CheckInterfacesException(fmt::format("interface {} already declared", name));
        }

        // remember interface name, so we will handle 2 empty interfaces with the same name (throw interface already declared exception)
        compilerRuntime->interfaceMethods[name] = {};

        if (node->hasParents()) {
            for (auto &parent: node->parents) {
                if (!compilerRuntime->interfaceMethods.contains(parent)) {
                    throw CheckInterfacesException(fmt::format("interface {} not found", parent));
                }

                for (auto &[methodName, methodDecl]: compilerRuntime->interfaceMethods[parent]) {
                    addMethodToInterface(name, methodDecl);
                }

                // cache extended interfaces
                auto &extendedInterfaces = compilerRuntime->implementedInterfaces[name];
                auto &parentExtendedInterfaces = compilerRuntime->implementedInterfaces[parent];
                extendedInterfaces.insert(parentExtendedInterfaces.cbegin(), parentExtendedInterfaces.cend());
                extendedInterfaces.insert(parent);
            }
        }

        for (auto &[methodName, methodDecl]: node->methods) {
            addMethodToInterface(name, methodDecl);
        }
    }

    void CheckInterfaces::addMethodToInterface(const std::string &interfaceName, MethodDeclNode *node) {
        auto &methodName = node->fnDecl->name;

        if (node->accessModifier != AccessModifier::PUBLIC) {
            throw CheckInterfacesException(fmt::format("interface method {}::{} must be public", interfaceName, methodName));
        }

        auto &methods = compilerRuntime->interfaceMethods[interfaceName];
        auto [it, inserted] = methods.insert({methodName, node});
        if (!inserted) {
            if (*it->second != *node) {
                throw CheckInterfacesException(fmt::format("interface method {}::{} is incompatible", interfaceName, methodName));
            }
        }
    }

    void CheckInterfaces::checkClass(ClassNode *node) {
        auto &name = node->name;
        classMethods[name] = node->methods;
        auto &klassMethods = classMethods[name];
        auto &classImplementedInterfaces = compilerRuntime->implementedInterfaces[name];
        std::unordered_set<std::string> interfacesToImplement(node->interfaces.cbegin(), node->interfaces.cend());

        if (node->hasParent()) {
            auto &parentName = node->parent;
            auto &parentClassMethods = classMethods[parentName];
            klassMethods.insert(parentClassMethods.cbegin(), parentClassMethods.cend());
            auto &parentImplementedInterfaces = compilerRuntime->implementedInterfaces[parentName];
            classImplementedInterfaces.insert(parentImplementedInterfaces.begin(), parentImplementedInterfaces.end());

            if (abstractClasses.contains(parentName)) {
                interfacesToImplement.insert(parentImplementedInterfaces.cbegin(), parentImplementedInterfaces.cend());
            }
        }

        if (node->abstract) {
            abstractClasses.insert(name);
        }

        for (auto &interfaceName: interfacesToImplement) {
            auto methods = compilerRuntime->interfaceMethods.find(interfaceName);
            if (methods == compilerRuntime->interfaceMethods.end()) {
                throw CheckInterfacesException(fmt::format("interface {} not found", interfaceName));
            }

            // abstract classes don't need to implement interfaces
            if (!node->abstract) {
                for (auto &[methodName, methodDecl]: methods->second) {
                    auto methodIt = klassMethods.find(methodName);
                    if (methodIt == klassMethods.cend()) {
                        throw CheckInterfacesException(fmt::format("interface method {}::{} must be implemented", interfaceName, methodName));
                    }

                    if (*methodIt->second != *methodDecl) {
                        throw CheckInterfacesException(
                                fmt::format("declaration of {}::{} must be compatible with interface {}", name,
                                            methodIt->second->fnDef->decl->name, interfaceName));
                    }
                }
            }

            // cache implemented interfaces
            auto &interfaceExtendedInterfaces = compilerRuntime->implementedInterfaces[interfaceName];
            classImplementedInterfaces.insert(interfaceExtendedInterfaces.cbegin(), interfaceExtendedInterfaces.cend());
            classImplementedInterfaces.insert(interfaceName);
        }
    }
}
