#pragma once

#include "config/models.h"
#include "policy/policy_engine.h"

#include <map>
#include <memory>
#include <regex>
#include <string>
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
    struct CompiledRoute {
        std::regex pattern;
        std::vector<std::string> methods;  // empty = all methods
        size_t routerIndex;
    };

    std::shared_ptr<const config::ConfigStore> store_;
    PolicyEngine engine_;
    std::vector<CompiledRoute> compiledRoutes_;
};

} // namespace kimchi
