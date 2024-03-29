#pragma once

#include <string>

namespace X {
    class Mangler {
        static inline const std::string INTERNAL_PREFIX = "x.";

    public:
        std::string mangleClass(const std::string &className) const {
            return "class." + className;
        }

        std::string unmangleClass(const std::string &mangledClassName) const {
            return mangledClassName.substr(std::strlen("class."));
        }

        bool isMangledClass(const std::string &name) const {
            return name.starts_with("class.");
        }

        std::string mangleInterface(const std::string &interfaceName) const {
            return "interface." + interfaceName;
        }

        bool isMangledInterface(const std::string &name) const {
            return name.starts_with("interface.");
        }

        std::string mangleMethod(const std::string &mangledClassName, const std::string &methodName) const {
            return mangledClassName + "_" + methodName;
        }

        std::string mangleInternalMethod(const std::string &mangledClassName, const std::string &methodName) const {
            return INTERNAL_PREFIX + mangleMethod(mangledClassName, methodName);
        }

        std::string mangleStaticProp(const std::string &mangledClassName, const std::string &propName) const {
            return mangledClassName + "_" + propName;
        }

        std::string mangleInternalFunction(const std::string &fnName) const {
            return INTERNAL_PREFIX + fnName;
        }

        std::string mangleInternalSymbol(const std::string &symbol) const {
            return INTERNAL_PREFIX + symbol;
        }
    };
}
