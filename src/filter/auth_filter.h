#pragma once

#include "filter/filter_base.h"

namespace kimchi {

class AuthFilter : public FilterBase {
public:
    explicit AuthFilter(proxygen::RequestHandler* next)
        : FilterBase(next, FailurePolicy::FAIL_CLOSE) {}

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;
};

} // namespace kimchi
