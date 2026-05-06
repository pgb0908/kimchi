#include "handler/gateway_handler_factory.h"
#include "handler/gateway_handler.h"
#include "filter/auth_filter.h"
#include "filter/header_filter.h"
#include "filter/rate_limit_filter.h"

namespace kimchi {

GatewayHandlerFactory::GatewayHandlerFactory(
    std::shared_ptr<const config::ConfigStore> store)
    : store_(std::move(store)),
      rateLimitStore_(std::make_shared<NullRateLimitStore>()) {}

void GatewayHandlerFactory::onServerStart(folly::EventBase* /*evb*/) noexcept {}

void GatewayHandlerFactory::onServerStop() noexcept {}

proxygen::RequestHandler* GatewayHandlerFactory::onRequest(
    proxygen::RequestHandler* /*upstream*/,
    proxygen::HTTPMessage* /*msg*/) noexcept {
    auto* handler = new GatewayHandler(store_);
    auto* rate    = new RateLimitFilter(handler, rateLimitStore_);
    auto* auth    = new AuthFilter(rate);
    auto* hdr     = new HeaderFilter(auth);
    return hdr;
}

} // namespace kimchi
