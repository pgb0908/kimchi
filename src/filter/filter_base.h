#pragma once

#include <string>

#include <proxygen/httpserver/Filters.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace kimchi {

class FilterBase : public proxygen::Filter {
public:
    enum class FailurePolicy { FAIL_OPEN, FAIL_CLOSE };

    explicit FilterBase(proxygen::RequestHandler* next, FailurePolicy policy)
        : proxygen::Filter(next), policy_(policy) {}

    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override {
        if (!blocked_) Filter::onBody(std::move(body));
    }

    void onEOM() noexcept override {
        if (!blocked_) Filter::onEOM();
    }

    void onUpgrade(proxygen::UpgradeProtocol prot) noexcept override {
        if (!blocked_) Filter::onUpgrade(prot);
    }

protected:
    void blockRequest(uint16_t status, const std::string& reason,
                      const std::string& body) noexcept {
        blocked_ = true;
        proxygen::ResponseBuilder(downstream_)
            .status(status, reason)
            .header("Content-Type", "application/json")
            .body(body)
            .sendWithEOM();
    }

    FailurePolicy policy_;
    bool blocked_{false};
};

} // namespace kimchi
