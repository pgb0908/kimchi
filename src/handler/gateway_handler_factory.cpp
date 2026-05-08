#include "handler/gateway_handler_factory.h"
#include "handler/gateway_handler.h"

#include <algorithm>

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

    size_t total = 0;
    for (size_t i = 0; i < store_->routers.size(); ++i) {
        const auto& router = store_->routers[i];
        for (const auto& rule : router.rules) {
            switch (rule.pathType) {
            case config::PathType::Exact:
                exactRoutes_[rule.path].push_back({rule.methods, i});
                ++total;
                break;
            case config::PathType::Prefix:
                prefixRoutes_.push_back({rule.path, rule.methods, i});
                ++total;
                break;
            case config::PathType::Regex:
                try {
                    regexRoutes_.push_back({std::regex(rule.path), rule.methods, i});
                    ++total;
                } catch (const std::regex_error& e) {
                    LOG(WARNING) << "Router '" << router.metadata.name
                                 << "': invalid regex '" << rule.path
                                 << "': " << e.what();
                }
                break;
            }
        }
    }

    // Longest prefix first — ensures more specific routes win.
    std::sort(prefixRoutes_.begin(), prefixRoutes_.end(),
              [](const PrefixRoute& a, const PrefixRoute& b) {
                  return a.prefix.size() > b.prefix.size();
              });

    LOG(INFO) << "GatewayHandlerFactory: " << total << " route(s) ["
              << exactRoutes_.size() << " exact, "
              << prefixRoutes_.size() << " prefix, "
              << regexRoutes_.size() << " regex] across "
              << store_->routers.size() << " router(s)";
}

void GatewayHandlerFactory::onServerStart(folly::EventBase* /*evb*/) noexcept {}
void GatewayHandlerFactory::onServerStop() noexcept {}

bool GatewayHandlerFactory::methodMatches(
    const std::vector<std::string>& allowed, const std::string& method) {
    return allowed.empty() ||
           std::any_of(allowed.begin(), allowed.end(),
                       [&](const std::string& m) { return m == method; });
}

proxygen::RequestHandler* GatewayHandlerFactory::onRequest(
    proxygen::RequestHandler* /*upstream*/,
    proxygen::HTTPMessage* msg) noexcept {

    const std::string& path = msg->getPath();
    const std::string& method = msg->getMethodString();

    const config::RouterConfig* matched = nullptr;

    // 1. Exact — O(1) hash lookup
    if (auto it = exactRoutes_.find(path); it != exactRoutes_.end()) {
        for (const auto& r : it->second) {
            if (methodMatches(r.methods, method)) {
                matched = &store_->routers[r.routerIndex];
                break;
            }
        }
    }

    // 2. Prefix — O(n), sorted longest-first so first match is most specific
    if (!matched) {
        for (const auto& r : prefixRoutes_) {
            if (path.starts_with(r.prefix) && methodMatches(r.methods, method)) {
                matched = &store_->routers[r.routerIndex];
                break;
            }
        }
    }

    // 3. Regex — O(n * m), sequential scan in registration order
    if (!matched) {
        for (const auto& r : regexRoutes_) {
            if (std::regex_match(path, r.pattern) && methodMatches(r.methods, method)) {
                matched = &store_->routers[r.routerIndex];
                break;
            }
        }
    }

    if (!matched) {
        return new NotFoundHandler();
    }

    auto* handler = new GatewayHandler(*matched, store_);
    return engine_.buildChain(*matched, handler);
}

} // namespace kimchi
