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

    static bool methodMatches(const std::vector<std::string>& allowed,
                               const std::string& method);

    std::shared_ptr<const config::ConfigStore> store_;
    PolicyEngine engine_;

    // Exact: O(1) hash lookup
    std::unordered_map<std::string, std::vector<ExactRoute>> exactRoutes_;
    // Prefix: sorted longest-first for longest-match semantics
    std::vector<PrefixRoute> prefixRoutes_;
    // Regex: sequential scan, registration order preserved
    std::vector<RegexRoute> regexRoutes_;
};

} // namespace kimchi
