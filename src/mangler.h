#ifndef X_MANGLER_H
#define X_MANGLER_H

#include <string>

namespace X {
    class Mangler {
        static inline const std::string INTERNAL_PREFIX = "x.";

    public:
        std::string mangleClass(const std::string &className) const {
            return "class." + className;
        }

        std::string mangleInterface(const std::string &interfaceName) const {
            return "interface." + interfaceName;
        }

        std::string unmangleClass(const std::string &mangledClassName) const {
            return mangledClassName.substr(std::strlen("class."));
        }

        std::string mangleMethod(const std::string &mangledClassName, const std::string &methodName) const {
            return mangledClassName + "_" + methodName;
        }

        std::string mangleStaticProp(const std::string &mangledClassName, const std::string &propName) const {
            return mangledClassName + "_" + propName;
        }

        std::string mangleInternalFunction(const std::string &fnName) const {
            return INTERNAL_PREFIX + fnName;
        }
    };
}

#endif //X_MANGLER_H
