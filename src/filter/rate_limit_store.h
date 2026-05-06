#pragma once

#include <string>

namespace kimchi {

struct RateLimitKey {
    std::string tenantId;
    std::string routeId;
};

class RateLimitStore {
public:
    virtual ~RateLimitStore() = default;

    // Returns true if request is allowed, false if quota exceeded.
    virtual bool checkAndDecrement(const RateLimitKey& key) = 0;
};

// Stub: always allows. Replace with Redis implementation.
class NullRateLimitStore : public RateLimitStore {
public:
    bool checkAndDecrement(const RateLimitKey& /*key*/) override {
        return true;
    }
};

} // namespace kimchi
