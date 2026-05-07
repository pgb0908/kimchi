#include "policy/jwt_policy.h"

#include <jwt-cpp/jwt.h>

#include <glog/logging.h>
#include <proxygen/lib/http/HTTPMessage.h>

namespace kimchi {

void JwtPolicy::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> msg) noexcept {

    const auto& cfg = jwks_->jwtConfig();

    // Extract token from Authorization: Bearer <token>
    const std::string authHeader =
        msg->getHeaders().getSingleOrEmpty("Authorization");

    if (authHeader.empty()) {
        blockRequest(401, "Unauthorized",
                     R"({"error":"unauthorized","message":"missing Authorization header"})");
        return;
    }

    static constexpr std::string_view kBearer = "Bearer ";
    if (authHeader.size() <= kBearer.size() ||
        authHeader.substr(0, kBearer.size()) != kBearer) {
        blockRequest(401, "Unauthorized",
                     R"({"error":"unauthorized","message":"Authorization must be Bearer token"})");
        return;
    }

    const std::string token = authHeader.substr(kBearer.size());

    try {
        auto decoded = jwt::decode(token);

        // Select key from JWKS by kid (falls back to first key if absent)
        auto jwks = jwt::parse_jwks(jwks_->jwksJson());

        std::string pubKeyPem;
        if (decoded.has_key_id()) {
            const std::string kid = decoded.get_key_id();
            if (!jwks.has_jwk(kid)) {
                blockRequest(401, "Unauthorized",
                             R"({"error":"unauthorized","message":"unknown key id"})");
                return;
            }
            pubKeyPem = jwks.get_jwk(kid).get_rsa_key();
        } else {
            auto keys = jwks.get_keys();
            if (keys.empty()) {
                blockRequest(401, "Unauthorized",
                             R"({"error":"unauthorized","message":"empty JWKS"})");
                return;
            }
            pubKeyPem = keys.front().get_rsa_key();
        }

        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::rs256(pubKeyPem));

        if (!cfg.issuer.empty()) {
            verifier = verifier.with_issuer(cfg.issuer);
        }
        if (!cfg.audience.empty()) {
            verifier = verifier.with_audience(cfg.audience);
        }

        verifier.verify(decoded);

        // Inject verified identity into upstream headers
        auto& hdrs = msg->getHeaders();
        hdrs.set("x-jwt-subject", decoded.get_subject());
        if (decoded.has_issuer()) {
            hdrs.set("x-jwt-issuer", decoded.get_issuer());
        }
        hdrs.set("x-auth-method", "jwt");
        // Remove original Authorization header so upstream doesn't re-validate
        hdrs.remove("Authorization");

        Filter::onRequest(std::move(msg));

    } catch (const jwt::error::token_verification_exception& e) {
        LOG(INFO) << "JWT verification failed: " << e.what();
        blockRequest(401, "Unauthorized",
                     R"({"error":"unauthorized","message":"token verification failed"})");
    } catch (const std::exception& e) {
        LOG(WARNING) << "JWT processing error: " << e.what();
        blockRequest(401, "Unauthorized",
                     R"({"error":"unauthorized","message":"invalid token"})");
    }
}

} // namespace kimchi
