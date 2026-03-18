#include "apollo/protocol/nng_wrapper.hpp"
#include <string>

#ifdef APOLLO_USE_NNG
#include <nng/nng.h>

namespace apollo {
namespace protocol {

//==============================================================================
// NngError 实现
//==============================================================================

std::error_category const& NngError::nng_category() {
    static class NngErrorCategory : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "nng";
        }

        std::string message(int condition) const override {
            return nng_strerror(condition);
        }
    } instance;
    return instance;
}

} // namespace protocol
} // namespace apollo

#else // !APOLLO_USE_NNG

namespace apollo {
namespace protocol {

std::error_category const& NngError::nng_category() {
    static class NngErrorCategory : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "nng_stub";
        }

        std::string message(int condition) const override {
            return "NNG not available (stub implementation)";
        }
    } instance;
    return instance;
}

} // namespace protocol
} // namespace apollo

#endif // APOLLO_USE_NNG
