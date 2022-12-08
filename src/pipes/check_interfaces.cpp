#include "check_interfaces.h"

#include <fmt/core.h>

#include "utils.h"

namespace X::Pipes {
    StatementListNode *CheckInterfaces::handle(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            if (auto interfaceNode = llvm::dyn_cast<InterfaceNode>(child)) {
                addInterface(interfaceNode);
            } else if (auto classNode = llvm::dyn_cast<ClassNode>(child)) {
                checkClass(classNode);
            }
        }

        return node;
    }

    void CheckInterfaces::addInterface(InterfaceNode *node) {
        auto &name = node->getName();

        if (compilerRuntime.interfaceMethods.contains(name)) {
            throw CheckInterfacesException(fmt::format("interface {} already declared", name));
        }

        // remember interface name, so we will handle 2 empty interfaces with the same name (throw interface already declared exception)
        compilerRuntime.interfaceMethods[name] = {};

        if (node->hasParents()) {
            for (auto &parent: node->getParents()) {
                if (!compilerRuntime.interfaceMethods.contains(parent)) {
                    throw CheckInterfacesException(fmt::format("interface {} not found", parent));
                }

                for (auto &[methodName, methodDecl]: compilerRuntime.interfaceMethods[parent]) {
                    addMethodToInterface(name, methodDecl);
                }

                // cache extended interfaces
                auto &extendedInterfaces = compilerRuntime.implementedInterfaces[name];
                extendedInterfaces.merge(compilerRuntime.implementedInterfaces[parent]);
                extendedInterfaces.insert(parent);
            }
        }

        for (auto &[methodName, methodDecl]: node->getMethods()) {
            addMethodToInterface(name, methodDecl);
        }
    }

    void CheckInterfaces::addMethodToInterface(const std::string &interfaceName, MethodDeclNode *node) {
        auto &methodName = node->getFnDecl()->getName();

        if (node->getAccessModifier() != AccessModifier::PUBLIC) {
            throw CheckInterfacesException(fmt::format("interface method {}::{} must be public", interfaceName, methodName));
        }

        auto &methods = compilerRuntime.interfaceMethods[interfaceName];
        // todo maybe insert and then check if value is created because "collision" probability is low
        auto methodDecl = methods.find(node->getFnDecl()->getName());
        if (methodDecl == methods.cend()) {
            methods[methodName] = node;
        } else if (*methodDecl->second != *node) {
            throw CheckInterfacesException(fmt::format("interface method {}::{} is incompatible", interfaceName, methodName));
        }
    }

    void CheckInterfaces::checkClass(ClassNode *node) {
        auto &name = node->getName();
        classMethods[name] = node->getMethods();
        auto &klassMethods = classMethods[name];
        if (node->hasParent()) {
            auto &parentName = node->getParent();
            klassMethods.merge(classMethods[parentName]);
            compilerRuntime.implementedInterfaces[name].merge(compilerRuntime.implementedInterfaces[parentName]);
        }

        for (auto &interfaceName: node->getInterfaces()) {
            auto methods = compilerRuntime.interfaceMethods.find(interfaceName);
            if (methods == compilerRuntime.interfaceMethods.end()) {
                throw CheckInterfacesException(fmt::format("interface {} not found", interfaceName));
            }

            for (auto &[methodName, methodDecl]: methods->second) {
                auto methodIt = klassMethods.find(methodName);
                if (methodIt == klassMethods.cend()) {
                    throw CheckInterfacesException(fmt::format("interface method {}::{} must be implemented", interfaceName, methodName));
                }

                if (!compareDeclAndDef(methodDecl, methodIt->second)) {
                    throw CheckInterfacesException(
                            fmt::format("declaration of {}::{} must be compatible with interface {}", name,
                                        methodIt->second->getFnDef()->getName(), interfaceName));
                }
            }

            // cache implemented interfaces
            auto &classImplementedInterfaces = compilerRuntime.implementedInterfaces[name];
            classImplementedInterfaces.merge(compilerRuntime.implementedInterfaces[interfaceName]);
            classImplementedInterfaces.insert(interfaceName);
        }
    }
}
