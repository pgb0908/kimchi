#pragma once

#include "filter/filter_base.h"

#include <string>

namespace kimchi {

class HeaderFilter : public FilterBase {
public:
    explicit HeaderFilter(proxygen::RequestHandler* next)
        : FilterBase(next, FailurePolicy::FAIL_OPEN) {}

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

private:
    static std::string generateRequestId() noexcept;
};

} // namespace kimchi
