#include "filter/rate_limit_filter.h"

namespace kimchi {

void RateLimitFilter::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> headers) noexcept {

    const auto& hdrs = headers->getHeaders();
    RateLimitKey key{
        .tenantId = hdrs.getSingleOrEmpty("x-tenant-id"),
        .routeId  = hdrs.getSingleOrEmpty("x-route-id"),
    };

    bool allowed = [&]() noexcept -> bool {
        try {
            return store_->checkAndDecrement(key);
        } catch (...) {
            return policy_ == FailurePolicy::FAIL_OPEN;
        }
    }();

    if (!allowed) {
        blockRequest(429, "Too Many Requests",
                     R"({"error":"rate_limit_exceeded","message":"too many requests"})");
        return;
    }

    Filter::onRequest(std::move(headers));
}

} // namespace kimchi
