#pragma once

#include "ApplicationContext.h"
#include <string>

#define REGISTER_COMPONENT(ClassName) \
    namespace { \
        struct ClassName##Registrar { \
            ClassName##Registrar() { \
                Apollo::ApplicationContext::getInstance().registerComponent<ClassName>(); \
            } \
        }; \
        static ClassName##Registrar g_##ClassName##Registrar; \
    }

#define DECLARE_COMPONENT(ClassName, ComponentName) \
public: \
    static const std::string& getStaticName() { \
        static std::string name = ComponentName; \
        return name; \
    }

namespace Apollo {

template<typename T>
class AutoRegister {
public:
    AutoRegister() {
        ApplicationContext::getInstance().registerComponent<T>();
    }
};

}