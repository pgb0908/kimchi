#include "handler/gateway_handler_factory.h"
#include "handler/gateway_handler.h"

namespace kimchi {

GatewayHandlerFactory::GatewayHandlerFactory(
    std::shared_ptr<const config::ConfigStore> store)
    : store_(std::move(store)) {}

void GatewayHandlerFactory::onServerStart(folly::EventBase* /*evb*/) noexcept {}

void GatewayHandlerFactory::onServerStop() noexcept {}

proxygen::RequestHandler* GatewayHandlerFactory::onRequest(
    proxygen::RequestHandler* /*upstream*/,
    proxygen::HTTPMessage* /*msg*/) noexcept {
    return new GatewayHandler(store_);
}

} // namespace kimchi
