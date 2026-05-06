#include "filter/header_filter.h"

#include <fmt/format.h>
#include <folly/Random.h>

namespace kimchi {

void HeaderFilter::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> headers) noexcept {

    auto& hdrs = headers->getHeaders();

    if (hdrs.getSingleOrEmpty("x-request-id").empty()) {
        hdrs.set("x-request-id", generateRequestId());
    }
    if (hdrs.getSingleOrEmpty("x-trace-id").empty()) {
        hdrs.set("x-trace-id", hdrs.getSingleOrEmpty("x-request-id"));
    }

    Filter::onRequest(std::move(headers));
}

std::string HeaderFilter::generateRequestId() noexcept {
    return fmt::format("{:016x}{:016x}",
                       folly::Random::rand64(),
                       folly::Random::rand64());
}

} // namespace kimchi
