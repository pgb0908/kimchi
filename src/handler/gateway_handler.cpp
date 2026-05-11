#include "handler/gateway_handler.h"
#include "handler/upstream_client.h"

#include <glog/logging.h>
#include <fmt/format.h>
#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace kimchi {

GatewayHandler::GatewayHandler(config::RouterConfig router,
                               const config::ServiceConfig* service,
                               folly::SocketAddress upstreamAddr)
    : router_(std::move(router)),
      service_(service),
      upstreamAddr_(std::move(upstreamAddr)) {}

void GatewayHandler::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> req) noexcept {
    LOG(INFO) << req->getMethodString() << " " << req->getPath();

    const auto& dest = router_.destinations[0];
    const auto& target = service_->loadBalancing.targets[0];

    auto upstreamReq = std::make_unique<proxygen::HTTPMessage>();
    upstreamReq->setMethod(req->getMethodString());
    upstreamReq->setHTTPVersion(1, 1);

    if (dest.rewrite && !dest.rewrite->path.empty()) {
        std::string upstreamUrl = dest.rewrite->path;
        const std::string& qs = req->getQueryString();
        if (!qs.empty()) upstreamUrl += "?" + qs;
        upstreamReq->setURL(upstreamUrl);
    } else {
        upstreamReq->setURL(req->getURL());
    }

    req->getHeaders().forEach(
        [&](const std::string& name, const std::string& val) {
            upstreamReq->getHeaders().add(name, val);
        });

    std::string hostHeader;
    if (dest.rewrite && !dest.rewrite->host.empty()) {
        hostHeader = dest.rewrite->host;
    } else {
        bool isDefaultPort =
            (service_->protocol == "HTTP"  && target.port == 80) ||
            (service_->protocol == "HTTPS" && target.port == 443);
        hostHeader = isDefaultPort
            ? target.host
            : fmt::format("{}:{}", target.host, target.port);
    }
    upstreamReq->getHeaders().set(proxygen::HTTP_HEADER_HOST, hostHeader);

    folly::EventBase* evb = folly::EventBaseManager::get()->getEventBase();
    upstreamClient_ = new UpstreamClient(downstream_, evb, std::move(upstreamReq));
    upstreamClient_->connect(upstreamAddr_);
}

void GatewayHandler::onBody(
    std::unique_ptr<folly::IOBuf> body) noexcept {
    upstreamClient_->sendBody(std::move(body));
}

void GatewayHandler::onEOM() noexcept {
    upstreamClient_->sendEOM();
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
