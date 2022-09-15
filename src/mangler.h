#ifndef X_MANGLER_H
#define X_MANGLER_H

#include <string>

namespace X {
    class Mangler {
    public:
        const std::string mangleClass(const std::string &className) const {
            return std::move("class." + className);
        }

        const std::string unmangleClass(const std::string &mangledClassName) const {
            return std::move(mangledClassName.substr(std::strlen("class.")));
        }

        const std::string mangleMethod(const std::string &mangledClassName, const std::string &methodName) const {
            return std::move(mangledClassName + "_" + methodName);
        }

        const std::string mangleStaticProp(const std::string &mangledClassName, const std::string &propName) const {
            return std::move(mangledClassName + "_" + propName);
        }
    };
}

#endif //X_MANGLER_H
