#pragma once

#include "config/models.h"
#include "filter/rate_limit_store.h"
#include "policy/jwks_cache.h"

#include <map>
#include <memory>
#include <string>

#include <proxygen/httpserver/RequestHandler.h>

namespace kimchi {

// Builds a per-request Proxygen filter chain based on PolicyConfig resources
// referenced by the matched RouterConfig.
//
// Chain structure (outermost → innermost):
//   HeaderFilter → [security policies by order] → RateLimitFilter → terminal
class PolicyEngine {
public:
    PolicyEngine(std::shared_ptr<const config::ConfigStore> store,
                 std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches,
                 std::shared_ptr<RateLimitStore> rateLimitStore);

    // Returns the head of the filter chain wrapping terminal.
    // Caller must not delete the returned pointer — Proxygen owns it.
    proxygen::RequestHandler* buildChain(
        const config::RouterConfig& router,
        proxygen::RequestHandler* terminal) const;

private:
    std::shared_ptr<const config::ConfigStore> store_;
    std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches_;
    std::shared_ptr<RateLimitStore> rateLimitStore_;
};

} // namespace kimchi
