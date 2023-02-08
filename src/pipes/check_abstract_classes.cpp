#include "check_abstract_classes.h"

#include <fmt/core.h>

#include "utils.h"

namespace X::Pipes {
    TopStatementListNode *CheckAbstractClasses::handle(TopStatementListNode *node) {
        for (auto klass: node->getClasses()) {
            checkClass(klass);
        }

        return node;
    }

    void CheckAbstractClasses::checkClass(ClassNode *node) {
        auto &className = node->getName();
        auto &parentClassName = node->getParent();
        auto &abstractMethods = node->getAbstractMethods();

        if (!abstractMethods.empty() && !node->isAbstract()) {
            throw CheckAbstractClassesException(fmt::format("class {} must be declared abstract", className));
        }

        if (node->isAbstract()) {
            if (classAbstractMethods.contains(className)) {
                throw CheckAbstractClassesException(fmt::format("class {} already exists", className));
            }

            if (node->hasParent()) {
                // copy abstract methods from parent class
                classAbstractMethods[className] = classAbstractMethods[parentClassName];
            }

            classAbstractMethods[className].insert(abstractMethods.cbegin(), abstractMethods.cend());

            return;
        }

        // if class has no parent or parent class has no abstract methods then return
        if (!node->hasParent() || classAbstractMethods[parentClassName].empty()) {
            return;
        }

        auto classMethods = node->getMethods();
        for (auto &[methodName, methodDecl]: classAbstractMethods[parentClassName]) {
            auto methodDef = classMethods.find(methodName);

            if (methodDef == classMethods.cend()) {
                throw CheckAbstractClassesException(fmt::format("abstract method {}::{} must be implemented", parentClassName, methodName));
            }

            if (!compareDeclAndDef(methodDecl, methodDef->second)) {
                throw CheckAbstractClassesException(fmt::format("declaration of {}::{} must be compatible with abstract class {}",
                                                                className, methodDef->second->getFnDef()->getName(), parentClassName));
            }
        }
    }
}
