#pragma once

#include "interfaces/http_ingress.hpp"

#include <string>

namespace interfaces {

class CallbackVerifier {
public:
    enum class Reason {
        SecretNotConfigured,
        SignatureHeaderMissing,
        SignatureHeaderAmbiguous,
        SignatureHeaderMalformed,
        SignatureMalformed,
        ContentLengthMissing,
        ContentLengthMismatch,
        RawFrameMismatch,
        SignatureMismatch,
    };

    struct Result {
        bool valid{false};
        Reason reason{Reason::SignatureMismatch};
        int status{401};
    };

    explicit CallbackVerifier(std::string secret);

    Result verify(const HttpRequest& request) const;

private:
    std::string secret_;
};

} // namespace interfaces
