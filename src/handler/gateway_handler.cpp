#include "handler/gateway_handler.h"
#include "handler/upstream_client.h"

#include <glog/logging.h>
#include <fmt/format.h>
#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace kimchi {

GatewayHandler::GatewayHandler(config::RouterConfig router,
                               std::shared_ptr<const config::ConfigStore> store)
    : router_(std::move(router)), store_(std::move(store)) {}

void GatewayHandler::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> req) noexcept {
    requestHeaders_ = std::move(req);
    LOG(INFO) << requestHeaders_->getMethodString() << " "
              << requestHeaders_->getPath();
}

void GatewayHandler::onBody(
    std::unique_ptr<folly::IOBuf> body) noexcept {
    if (!requestBody_) {
        requestBody_ = std::move(body);
    } else {
        requestBody_->appendToChain(std::move(body));
    }
}

void GatewayHandler::onEOM() noexcept {
    if (router_.destinations.empty()) {
        proxygen::ResponseBuilder(downstream_)
            .status(502, "Bad Gateway")
            .header("Content-Type", "application/json")
            .body(R"({"error":"bad_gateway","message":"router has no destinations"})")
            .sendWithEOM();
        return;
    }

    const auto& dest = router_.destinations[0];

    // Service lookup
    const config::ServiceConfig* svc = nullptr;
    for (const auto& s : store_->services) {
        if (s.metadata.name == dest.destinationName) {
            svc = &s;
            break;
        }
    }

    if (!svc || svc->loadBalancing.targets.empty()) {
        LOG(ERROR) << "upstream service not found: " << dest.destinationName;
        proxygen::ResponseBuilder(downstream_)
            .status(502, "Bad Gateway")
            .header("Content-Type", "application/json")
            .body(R"({"error":"bad_gateway","message":"upstream service not configured"})")
            .sendWithEOM();
        return;
    }

    const auto& target = svc->loadBalancing.targets[0];

    auto upstreamReq = std::make_unique<proxygen::HTTPMessage>();
    upstreamReq->setMethod(requestHeaders_->getMethodString());
    upstreamReq->setHTTPVersion(1, 1);

    if (dest.rewrite && !dest.rewrite->path.empty()) {
        std::string upstreamUrl = dest.rewrite->path;
        const std::string& qs = requestHeaders_->getQueryString();
        if (!qs.empty()) upstreamUrl += "?" + qs;
        upstreamReq->setURL(upstreamUrl);
    } else {
        upstreamReq->setURL(requestHeaders_->getURL());
    }

    requestHeaders_->getHeaders().forEach(
        [&](const std::string& name, const std::string& val) {
            upstreamReq->getHeaders().add(name, val);
        });

    std::string hostHeader;
    if (dest.rewrite && !dest.rewrite->host.empty()) {
        hostHeader = dest.rewrite->host;
    } else {
        bool isDefaultPort =
            (svc->protocol == "HTTP" && target.port == 80) ||
            (svc->protocol == "HTTPS" && target.port == 443);
        hostHeader = isDefaultPort
            ? target.host
            : fmt::format("{}:{}", target.host, target.port);
    }
    upstreamReq->getHeaders().set(proxygen::HTTP_HEADER_HOST, hostHeader);

    folly::EventBase* evb = folly::EventBaseManager::get()->getEventBase();
    folly::SocketAddress addr;
    try {
        addr.setFromHostPort(target.host, target.port);
    } catch (const std::exception& e) {
        LOG(ERROR) << "invalid upstream address " << target.host << ":"
                   << target.port << ": " << e.what();
        proxygen::ResponseBuilder(downstream_)
            .status(502, "Bad Gateway")
            .header("Content-Type", "application/json")
            .body(R"({"error":"bad_gateway","message":"invalid upstream address"})")
            .sendWithEOM();
        return;
    }

    auto* client = new UpstreamClient(downstream_, evb, std::move(upstreamReq),
                                      std::move(requestBody_));
    client->connect(addr);
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
