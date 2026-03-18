#include "apollo/protocol/nng_wrapper.hpp"
#include <string>

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
