#include "filter/auth_filter.h"

namespace kimchi {

void AuthFilter::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> headers) noexcept {

    const auto& hdrs = headers->getHeaders();
    const auto apiKey   = hdrs.getSingleOrEmpty("x-api-key");
    const auto authHdr  = hdrs.getSingleOrEmpty("Authorization");

    if (apiKey.empty() && authHdr.empty()) {
        blockRequest(401, "Unauthorized",
                     R"({"error":"unauthorized","message":"missing credentials"})");
        return;
    }

    auto& mutableHdrs = headers->getHeaders();
    if (!apiKey.empty()) {
        mutableHdrs.set("x-auth-method",  "api-key");
        mutableHdrs.set("x-auth-subject", apiKey);
    } else {
        mutableHdrs.set("x-auth-method", "bearer");
        auto token = authHdr.starts_with("Bearer ")
            ? authHdr.substr(7) : authHdr;
        mutableHdrs.set("x-auth-subject", token);
    }
    mutableHdrs.set("x-gateway-decision", "pass");

    Filter::onRequest(std::move(headers));
}

} // namespace kimchi
