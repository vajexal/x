#include "check_interfaces.h"

#include <fmt/core.h>

#include "utils.h"

namespace X::Pipes {
    StatementListNode *CheckInterfaces::handle(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            if (auto interfaceNode = dynamic_cast<InterfaceNode *>(child)) {
                if (interfaces.contains(interfaceNode->getName())) {
                    throw CheckInterfacesException(fmt::format("interface {} already declared", interfaceNode->getName()));
                }

                interfaces[interfaceNode->getName()] = interfaceNode;
            } else if (auto classNode = dynamic_cast<ClassNode *>(child)) {
                auto classMethods = classNode->getMembers()->getMethods();

                for (auto &interfaceName: classNode->getInterfaces()) {
                    auto interface = interfaces.find(interfaceName);
                    if (interface == interfaces.end()) {
                        throw CheckInterfacesException(fmt::format("interface {} not found", interfaceName));
                    }

                    for (auto interfaceMethod: interface->second->getMethods()) {
                        // todo optimize
                        auto classMethod = std::find_if(classMethods.cbegin(), classMethods.cend(), [interfaceMethod](MethodDefNode *method) {
                            return method->getFnDef()->getName() == interfaceMethod->getFnDecl()->getName();
                        });
                        if (classMethod == classMethods.cend()) {
                            throw CheckInterfacesException(
                                    fmt::format("interface method {}::{} must be implemented", interfaceName, interfaceMethod->getFnDecl()->getName()));
                        }

                        if (!compareDeclAndDef(interfaceMethod, *classMethod)) {
                            throw CheckInterfacesException(
                                    fmt::format("declaration of {}::{} must be compatible with interface {}", classNode->getName(),
                                                (*classMethod)->getFnDef()->getName(), interfaceName));
                        }
                    }
                }
            }
        }

        return node;
    }
}

