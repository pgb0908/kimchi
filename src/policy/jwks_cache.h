#pragma once

#include "config/models.h"

#include <memory>
#include <string>

namespace kimchi {

// Immutable JWKS snapshot fetched at gateway startup.
// v1: no background refresh — call fetch() again and replace if needed.
class JwksCache {
public:
    // Blocking HTTP GET of jwksUri. Throws std::runtime_error on failure.
    static std::shared_ptr<JwksCache> fetch(const config::JwtConfig& cfg);

    const config::JwtConfig& jwtConfig() const noexcept { return cfg_; }
    const std::string& jwksJson() const noexcept { return json_; }

private:
    JwksCache(config::JwtConfig cfg, std::string json)
        : cfg_(std::move(cfg)), json_(std::move(json)) {}

    config::JwtConfig cfg_;
    std::string json_;
};

} // namespace kimchi
