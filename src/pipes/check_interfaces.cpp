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

        if (interfaceMethods.contains(name)) {
            throw CheckInterfacesException(fmt::format("interface {} already declared", name));
        }

        // remember interface name, so we will handle 2 empty interfaces with the same name (throw interface already declared exception)
        interfaceMethods[name] = {};

        if (node->hasParents()) {
            for (auto &parent: node->getParents()) {
                if (!interfaceMethods.contains(parent)) {
                    throw CheckInterfacesException(fmt::format("interface {} not found", parent));
                }

                for (auto &[methodName, methodDecl]: interfaceMethods[parent]) {
                    addMethodToInterface(name, methodDecl);
                }
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

        auto &methods = interfaceMethods[interfaceName];
        auto methodDecl = methods.find(node->getFnDecl()->getName());
        if (methodDecl != methods.end() && *methodDecl->second != *node) {
            throw CheckInterfacesException(fmt::format("interface method {}::{} is incompatible", interfaceName, methodName));
        }

        methods[methodName] = node;
    }

    void CheckInterfaces::checkClass(ClassNode *node) {
        classMethods[node->getName()] = node->getMethods();
        auto &klassMethods = classMethods[node->getName()];
        if (node->hasParent()) {
            klassMethods.merge(classMethods[node->getParent()]);
        }

        for (auto &interfaceName: node->getInterfaces()) {
            auto methods = interfaceMethods.find(interfaceName);
            if (methods == interfaceMethods.end()) {
                throw CheckInterfacesException(fmt::format("interface {} not found", interfaceName));
            }

            for (auto &[methodName, methodDecl]: methods->second) {
                auto methodIt = klassMethods.find(methodName);
                if (methodIt == klassMethods.cend()) {
                    throw CheckInterfacesException(fmt::format("interface method {}::{} must be implemented", interfaceName, methodName));
                }

                if (!compareDeclAndDef(methodDecl, methodIt->second)) {
                    throw CheckInterfacesException(
                            fmt::format("declaration of {}::{} must be compatible with interface {}", node->getName(),
                                        methodIt->second->getFnDef()->getName(), interfaceName));
                }
            }
        }
    }
}
