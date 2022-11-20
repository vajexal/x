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
        auto &parentClassName = node->getParent();

        if (!node->getAbstractMethods().empty() && !node->isAbstract()) {
            throw CheckAbstractClassesException(fmt::format("class {} must be declared abstract", className));
        }

        if (node->isAbstract()) {
            if (classAbstractMethods.contains(className)) {
                throw CheckAbstractClassesException(fmt::format("class {} already declared", className));
            }

            if (node->hasParent()) {
                // copy abstract methods from parent class
                classAbstractMethods[className] = classAbstractMethods[parentClassName];
            }

            auto &abstractMethods = classAbstractMethods[className];
            for (auto methodDecl: node->getAbstractMethods()) {
                abstractMethods[methodDecl->getFnDecl()->getName()] = methodDecl;
            }

            return;
        }

        // if class has no parent or parent class has no abstract methods then return
        if (!node->hasParent() || classAbstractMethods[parentClassName].empty()) {
            return;
        }

        auto classMethods = node->getMethods();
        for (auto &[methodName, methodDecl]: classAbstractMethods[parentClassName]) {
            // todo optimize
            auto methodDef = std::find_if(classMethods.cbegin(), classMethods.cend(), [methodName = methodName](MethodDefNode *method) {
                return method->getFnDef()->getName() == methodName;
            });

            if (methodDef == classMethods.cend()) {
                throw CheckAbstractClassesException(fmt::format("abstract method {}::{} must be implemented", parentClassName, methodName));
            }

            if (!compareDeclAndDef(methodDecl, *methodDef)) {
                throw CheckAbstractClassesException(fmt::format("declaration of {}::{} must be compatible with abstract class {}",
                                                                className, (*methodDef)->getFnDef()->getName(), parentClassName));
            }
        }
    }
}
