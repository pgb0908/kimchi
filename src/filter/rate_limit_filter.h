#pragma once

#include "filter/filter_base.h"
#include "filter/rate_limit_store.h"

#include <memory>

namespace kimchi {

class RateLimitFilter : public FilterBase {
public:
    RateLimitFilter(proxygen::RequestHandler* next,
                    std::shared_ptr<RateLimitStore> store,
                    FailurePolicy policy = FailurePolicy::FAIL_OPEN)
        : FilterBase(next, policy), store_(std::move(store)) {}

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

private:
    std::shared_ptr<RateLimitStore> store_;
};

} // namespace kimchi
