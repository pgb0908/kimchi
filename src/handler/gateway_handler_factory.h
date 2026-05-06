#pragma once

#include "config/models.h"
#include "filter/rate_limit_store.h"

#include <memory>

#include <proxygen/httpserver/RequestHandlerFactory.h>

namespace kimchi {

class GatewayHandlerFactory : public proxygen::RequestHandlerFactory {
public:
    explicit GatewayHandlerFactory(std::shared_ptr<const config::ConfigStore> store);

    void onServerStart(folly::EventBase* evb) noexcept override;
    void onServerStop() noexcept override;

    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler* upstream,
        proxygen::HTTPMessage* msg) noexcept override;

private:
    std::shared_ptr<const config::ConfigStore> store_;
    std::shared_ptr<RateLimitStore> rateLimitStore_;
};

} // namespace kimchi
