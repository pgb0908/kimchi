#include "handler/gateway_handler.h"

#include <glog/logging.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace kimchi {

GatewayHandler::GatewayHandler(std::shared_ptr<const config::ConfigStore> store)
    : store_(std::move(store)) {}

void GatewayHandler::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> headers) noexcept {
    requestHeaders_ = std::move(headers);
    LOG(INFO) << requestHeaders_->getMethodString() << " "
              << requestHeaders_->getPath();
}

void GatewayHandler::onBody(std::unique_ptr<folly::IOBuf> /*body*/) noexcept {
    // body forwarding deferred to upstream proxy phase
}

void GatewayHandler::onEOM() noexcept {
    proxygen::ResponseBuilder(downstream_)
        .status(200, "OK")
        .header("Content-Type", "application/json")
        .body("{\"status\":\"ok\",\"message\":\"kimchi gateway\"}")
        .sendWithEOM();
}

void GatewayHandler::onUpgrade(proxygen::UpgradeProtocol /*prot*/) noexcept {
    proxygen::ResponseBuilder(downstream_)
        .status(400, "Bad Request")
        .sendWithEOM();
}

void GatewayHandler::requestComplete() noexcept {
    delete this;
}

void GatewayHandler::onError(proxygen::ProxygenError /*err*/) noexcept {
    delete this;
}

} // namespace kimchi
