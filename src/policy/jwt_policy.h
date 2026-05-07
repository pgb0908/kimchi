#pragma once

#include "filter/filter_base.h"
#include "policy/jwks_cache.h"

#include <memory>

namespace kimchi {

// Validates Bearer JWT tokens against a JWKS keyset.
// On success: injects x-jwt-subject and x-jwt-issuer headers upstream.
// On failure: returns 401 Unauthorized (FAIL_CLOSE).
class JwtPolicy : public FilterBase {
public:
    JwtPolicy(proxygen::RequestHandler* next, std::shared_ptr<JwksCache> jwks)
        : FilterBase(next, FailurePolicy::FAIL_CLOSE),
          jwks_(std::move(jwks)) {}

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> msg) noexcept override;

private:
    std::shared_ptr<JwksCache> jwks_;
};

} // namespace kimchi
