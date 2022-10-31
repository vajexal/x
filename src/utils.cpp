#include "utils.h"

namespace X {
    bool compareDeclAndDef(MethodDeclNode *declNode, MethodDefNode *defNode) {
        if (declNode->getAccessModifier() != defNode->getAccessModifier()) {
            return false;
        }

        if (declNode->getIsStatic() != defNode->getIsStatic()) {
            return false;
        }

        auto fnDecl = declNode->getFnDecl();
        auto fnDef = defNode->getFnDef();

        if (fnDecl->getReturnType() != fnDef->getReturnType()) {
            return false;
        }

        if (fnDecl->getArgs().size() != fnDef->getArgs().size()) {
            return false;
        }

        for (auto i = 0; i < fnDecl->getArgs().size(); i++) {
            if (*fnDecl->getArgs()[i] != *fnDef->getArgs()[i]) {
                return false;
            }
        }

        return true;
    }
}
