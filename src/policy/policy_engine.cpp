#include "policy/policy_engine.h"

#include "filter/header_filter.h"
#include "filter/rate_limit_filter.h"
#include "policy/jwt_policy.h"

#include <algorithm>
#include <vector>

namespace kimchi {

PolicyEngine::PolicyEngine(
    std::shared_ptr<const config::ConfigStore> store,
    std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches,
    std::shared_ptr<RateLimitStore> rateLimitStore)
    : store_(std::move(store)),
      jwksCaches_(std::move(jwksCaches)),
      rateLimitStore_(std::move(rateLimitStore)) {}

proxygen::RequestHandler* PolicyEngine::buildChain(
    const config::RouterConfig& router,
    proxygen::RequestHandler* terminal) const {

    // Collect active PolicyConfig objects for this router, sorted by order.
    std::vector<const config::PolicyConfig*> active;
    for (const auto& name : router.policies) {
        for (const auto& pc : store_->policies) {
            if (pc.metadata.name == name) {
                active.push_back(&pc);
                break;
            }
        }
    }
    std::sort(active.begin(), active.end(),
              [](const auto* a, const auto* b) {
                  return a->spec.order < b->spec.order;
              });

    // Build chain inside-out:
    // terminal ← RateLimitFilter ← [policies, highest order first] ← HeaderFilter

    proxygen::RequestHandler* current =
        new RateLimitFilter(terminal, rateLimitStore_);

    // Wrap in reverse order so lower-order (higher priority) policies
    // end up closer to the outermost HeaderFilter.
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
        const auto* pc = *it;
        if (!pc->spec.security) continue;
        if (!pc->spec.security->jwt) continue;

        auto cacheIt = jwksCaches_.find(pc->metadata.name);
        if (cacheIt == jwksCaches_.end()) continue;

        current = new JwtPolicy(current, cacheIt->second);
    }

    return new HeaderFilter(current);
}

} // namespace kimchi
