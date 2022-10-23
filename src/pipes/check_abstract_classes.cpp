#include "check_abstract_classes.h"

#include <fmt/core.h>

#include "utils.h"

namespace X::Pipes {
    StatementListNode *CheckAbstractClasses::handle(StatementListNode *node) {
        for (auto child: node->getChildren()) {
            if (auto classNode = dynamic_cast<ClassNode *>(child)) {
                checkClass(classNode);
            }
        }

        return node;
    }

    void CheckAbstractClasses::checkClass(ClassNode *node) {
        auto &className = node->getName();

        if (node->hasParent() && !classAbstractMethods.contains(node->getParent())) {
            throw CheckAbstractClassesException(fmt::format("class {} not found", node->getParent()));
        }

        if (node->isAbstract()) {
            if (classAbstractMethods.contains(className)) {
                throw CheckAbstractClassesException(fmt::format("class {} already declared", className));
            }

            if (node->hasParent()) {
                // copy abstract methods from parent class
                classAbstractMethods[className] = classAbstractMethods[node->getParent()];
            }

            auto &abstractMethods = classAbstractMethods[className];
            for (auto methodDecl: node->getMembers()->getAbstractMethods()) {
                abstractMethods[methodDecl->getFnDecl()->getName()] = methodDecl;
            }

            return;
        }

        // if class has no parent or parent class has no abstract methods then return
        if (!node->hasParent() || classAbstractMethods[node->getParent()].empty()) {
            return;
        }

        auto classMethods = node->getMembers()->getMethods();
        for (auto &[methodName, methodDecl]: classAbstractMethods[node->getParent()]) {
            // todo optimize
            auto methodDef = std::find_if(classMethods.cbegin(), classMethods.cend(), [methodName = methodName](MethodDefNode *method) {
                return method->getFnDef()->getName() == methodName;
            });

            if (methodDef == classMethods.cend()) {
                throw CheckAbstractClassesException(fmt::format("abstract method {}::{} must be implemented", node->getParent(), methodName));
            }

            if (!compareDeclAndDef(methodDecl, *methodDef)) {
                throw CheckAbstractClassesException(fmt::format("declaration of {}::{} must be compatible with abstract class {}",
                                                                className, (*methodDef)->getFnDef()->getName(), node->getParent()));
            }
        }
    }
}
