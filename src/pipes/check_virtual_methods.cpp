#include "check_virtual_methods.h"

#include <fmt/core.h>

namespace X::Pipes {
    StatementListNode *CheckVirtualMethods::handle(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            if (auto classNode = llvm::dyn_cast<ClassNode>(child)) {
                checkClass(classNode);
            }
        }

        return node;
    }

    void CheckVirtualMethods::checkClass(ClassNode *node) {
        auto &name = node->getName();

        classes[name] = node;

        if (!node->hasParent()) {
            return;
        }

        auto &parent = node->getParent();

        // cache extended classes
        auto &extendedClasses = compilerRuntime.extendedClasses[name];
        extendedClasses.merge(compilerRuntime.extendedClasses[parent]);
        extendedClasses.insert(parent);

        auto classMethods = node->getMethods();
        auto currentClass = getClass(parent);
        while (currentClass) {
            for (auto &[methodName, methodDef]: currentClass->getMethods()) {
                if (methodDef->getIsStatic() || methodName == CONSTRUCTOR_FN_NAME) {
                    continue;
                }

                auto classMethod = classMethods.find(methodName);
                if (classMethod == classMethods.cend()) {
                    continue;
                }

                if (*methodDef != *classMethod->second) {
                    throw CheckVirtualMethodsException(fmt::format("declaration of {}::{} must be compatible with {}::{}",
                                                                   name, methodName, currentClass->getName(), methodName));
                }

                compilerRuntime.virtualMethods[currentClass->getName()].insert(methodName);
            }

            if (!currentClass->hasParent()) {
                break;
            }

            currentClass = getClass(currentClass->getParent());
        }
    }

    ClassNode *CheckVirtualMethods::getClass(const std::string &className) const {
        auto klass = classes.find(className);
        if (klass == classes.cend()) {
            throw CheckVirtualMethodsException(fmt::format("class {} not found", className));
        }
        return klass->second;
    }
}
