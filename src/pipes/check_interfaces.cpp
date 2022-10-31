#include "check_interfaces.h"

#include <fmt/core.h>

#include "utils.h"

namespace X::Pipes {
    StatementListNode *CheckInterfaces::handle(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            if (auto interfaceNode = dynamic_cast<InterfaceNode *>(child)) {
                addInterface(interfaceNode);
            } else if (auto classNode = dynamic_cast<ClassNode *>(child)) {
                checkClass(classNode);
            }
        }

        return node;
    }

    void CheckInterfaces::addInterface(InterfaceNode *node) {
        if (interfaceMethods.contains(node->getName())) {
            throw CheckInterfacesException(fmt::format("interface {} already declared", node->getName()));
        }

        if (node->hasParents()) {
            for (auto &parent: node->getParents()) {
                if (!interfaceMethods.contains(parent)) {
                    throw CheckInterfacesException(fmt::format("interface {} not found", parent));
                }

                for (auto &[methodName, methodDecl]: interfaceMethods[parent]) {
                    addMethodToInterface(node->getName(), methodDecl);
                }
            }
        }

        for (auto methodDecl: node->getMethods()) {
            addMethodToInterface(node->getName(), methodDecl);
        }
    }

    void CheckInterfaces::addMethodToInterface(const std::string &interfaceName, MethodDeclNode *node) {
        auto &methodName = node->getFnDecl()->getName();
        auto &methods = interfaceMethods[interfaceName];
        auto methodDecl = methods.find(node->getFnDecl()->getName());
        if (methodDecl != methods.end() && *methodDecl->second != *node) {
            throw CheckInterfacesException(fmt::format("interface method {}::{} is incompatible", interfaceName, methodName));
        }

        methods[methodName] = node;
    }

    void CheckInterfaces::checkClass(ClassNode *node) {
        auto classMethods = node->getMembers()->getMethods();

        for (auto &interfaceName: node->getInterfaces()) {
            auto methods = interfaceMethods.find(interfaceName);
            if (methods == interfaceMethods.end()) {
                throw CheckInterfacesException(fmt::format("interface {} not found", interfaceName));
            }

            for (auto &[methodName, methodDecl]: methods->second) {
                // todo optimize
                auto classMethod = std::find_if(classMethods.cbegin(), classMethods.cend(), [methodName = methodName](MethodDefNode *method) {
                    return method->getFnDef()->getName() == methodName;
                });
                if (classMethod == classMethods.cend()) {
                    throw CheckInterfacesException(fmt::format("interface method {}::{} must be implemented", interfaceName, methodName));
                }

                if (!compareDeclAndDef(methodDecl, *classMethod)) {
                    throw CheckInterfacesException(
                            fmt::format("declaration of {}::{} must be compatible with interface {}", node->getName(),
                                        (*classMethod)->getFnDef()->getName(), interfaceName));
                }
            }
        }
    }
}

