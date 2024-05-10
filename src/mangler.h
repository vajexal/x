#pragma once

#include <string>

namespace X {
    class Mangler {
        static inline const std::string INTERNAL_PREFIX = "x.";

    public:
        std::string mangleClass(const std::string &className) {
            return "class." + className;
        }

        std::string mangleInterface(const std::string &interfaceName) {
            return "interface." + interfaceName;
        }

        std::string mangleMethod(const std::string &mangledClassName, const std::string &methodName) {
            return mangledClassName + '_' + methodName;
        }

        std::string mangleHiddenMethod(const std::string &mangledClassName, const std::string &methodName) {
            return mangledClassName + '.' + methodName;
        }

        std::string mangleInternalMethod(const std::string &mangledClassName, const std::string &methodName) {
            return INTERNAL_PREFIX + mangleMethod(mangledClassName, methodName);
        }

        std::string mangleStaticProp(const std::string &mangledClassName, const std::string &propName) {
            return mangledClassName + '_' + propName;
        }

        std::string mangleInternalFunction(const std::string &fnName) {
            return INTERNAL_PREFIX + fnName;
        }

        std::string mangleInternalSymbol(const std::string &symbol) {
            return INTERNAL_PREFIX + symbol;
        }
    };
}
