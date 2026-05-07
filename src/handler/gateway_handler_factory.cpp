#include "handler/gateway_handler_factory.h"
#include "handler/gateway_handler.h"

#include <glog/logging.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace kimchi {

namespace {

class NotFoundHandler : public proxygen::RequestHandler {
public:
    void onRequest(
        std::unique_ptr<proxygen::HTTPMessage> /*msg*/) noexcept override {
        proxygen::ResponseBuilder(downstream_)
            .status(404, "Not Found")
            .header("Content-Type", "application/json")
            .body(R"({"error":"not_found","message":"no route matched"})")
            .sendWithEOM();
    }
    void onBody(std::unique_ptr<folly::IOBuf> /*body*/) noexcept override {}
    void onEOM() noexcept override {}
    void onUpgrade(proxygen::UpgradeProtocol /*prot*/) noexcept override {}
    void requestComplete() noexcept override { delete this; }
    void onError(proxygen::ProxygenError /*err*/) noexcept override { delete this; }
};

} // namespace

GatewayHandlerFactory::GatewayHandlerFactory(
    std::shared_ptr<const config::ConfigStore> store,
    std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches)
    : store_(store),
      engine_(store, std::move(jwksCaches),
              std::make_shared<NullRateLimitStore>()) {

    // Pre-compile all router path regexes at startup to avoid per-request cost.
    for (size_t i = 0; i < store_->routers.size(); ++i) {
        const auto& router = store_->routers[i];
        for (const auto& rule : router.rules) {
            try {
                compiledRoutes_.push_back(
                    {std::regex(rule.path), rule.methods, i});
            } catch (const std::regex_error& e) {
                LOG(WARNING) << "Router '" << router.metadata.name
                             << "': invalid path regex '" << rule.path
                             << "': " << e.what();
            }
        }
    }

    LOG(INFO) << "GatewayHandlerFactory: " << compiledRoutes_.size()
              << " compiled route(s) across " << store_->routers.size()
              << " router(s)";
}

void GatewayHandlerFactory::onServerStart(folly::EventBase* /*evb*/) noexcept {}
void GatewayHandlerFactory::onServerStop() noexcept {}

proxygen::RequestHandler* GatewayHandlerFactory::onRequest(
    proxygen::RequestHandler* /*upstream*/,
    proxygen::HTTPMessage* msg) noexcept {

    const std::string& path = msg->getPath();
    const std::string& method = msg->getMethodString();

    const config::RouterConfig* matched = nullptr;
    for (const auto& route : compiledRoutes_) {
        if (!route.methods.empty()) {
            bool methodOk = false;
            for (const auto& m : route.methods) {
                if (m == method) { methodOk = true; break; }
            }
            if (!methodOk) continue;
        }
        if (std::regex_match(path, route.pattern)) {
            matched = &store_->routers[route.routerIndex];
            break;
        }
    }

    if (!matched) {
        return new NotFoundHandler();
    }

    auto* handler = new GatewayHandler(*matched, store_);
    return engine_.buildChain(*matched, handler);
}

} // namespace kimchi
