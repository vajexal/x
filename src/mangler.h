#pragma once

#include <string>

namespace X {
    class Mangler {
        static inline const std::string INTERNAL_PREFIX = "x.";

    public:
        static std::string mangleClass(const std::string &className) {
            return "class." + className;
        }

        static std::string mangleInterface(const std::string &interfaceName) {
            return "interface." + interfaceName;
        }

        static std::string mangleMethod(const std::string &mangledClassName, const std::string &methodName) {
            return mangledClassName + "_" + methodName;
        }

        static std::string mangleInternalMethod(const std::string &mangledClassName, const std::string &methodName) {
            return INTERNAL_PREFIX + mangleMethod(mangledClassName, methodName);
        }

        static std::string mangleStaticProp(const std::string &mangledClassName, const std::string &propName) {
            return mangledClassName + "_" + propName;
        }

        static std::string mangleInternalFunction(const std::string &fnName) {
            return INTERNAL_PREFIX + fnName;
        }

        static std::string mangleInternalSymbol(const std::string &symbol) {
            return INTERNAL_PREFIX + symbol;
        }
    };
}
