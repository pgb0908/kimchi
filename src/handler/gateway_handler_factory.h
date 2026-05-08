#pragma once

#include "config/models.h"
#include "policy/policy_engine.h"

#include <algorithm>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <folly/SocketAddress.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>

namespace kimchi {

class GatewayHandlerFactory : public proxygen::RequestHandlerFactory {
public:
    GatewayHandlerFactory(
        std::shared_ptr<const config::ConfigStore> store,
        std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches);

    void onServerStart(folly::EventBase* evb) noexcept override;
    void onServerStop() noexcept override;

    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler* upstream,
        proxygen::HTTPMessage* msg) noexcept override;

private:
    // Routing tables
    struct ExactRoute {
        std::vector<std::string> methods;
        size_t routerIndex;
    };
    struct PrefixRoute {
        std::string prefix;
        std::vector<std::string> methods;
        size_t routerIndex;
    };
    struct RegexRoute {
        std::regex pattern;
        std::vector<std::string> methods;
        size_t routerIndex;
    };

    // Service cache: pre-resolved at startup to eliminate per-request
    // DNS lookups and O(n) service scans.
    struct CachedService {
        const config::ServiceConfig* config;
        std::vector<folly::SocketAddress> addrs;  // one per target, same order
    };

    static bool methodMatches(const std::vector<std::string>& allowed,
                               const std::string& method);

    std::shared_ptr<const config::ConfigStore> store_;
    PolicyEngine engine_;

    // Routing
    std::unordered_map<std::string, std::vector<ExactRoute>> exactRoutes_;
    std::vector<PrefixRoute> prefixRoutes_;
    std::vector<RegexRoute> regexRoutes_;

    // Service lookup cache (name → pre-resolved)
    std::unordered_map<std::string, CachedService> serviceCache_;
};

} // namespace kimchi
