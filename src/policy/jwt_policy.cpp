#include "policy/jwt_policy.h"
#include "policy/folly_jwt_traits.h"

#include <glog/logging.h>
#include <proxygen/lib/http/HTTPMessage.h>

namespace kimchi {

namespace {
using JwtTraits = jwt::traits::folly_dynamic;
} // namespace

void JwtPolicy::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> msg) noexcept {

    const auto& cfg = jwks_->jwtConfig();

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
        auto decoded = jwt::decode<JwtTraits>(token);

        auto jwks = jwt::parse_jwks<JwtTraits>(jwks_->jwksJson());

        std::string pubKeyPem;
        if (decoded.has_key_id()) {
            const std::string kid = decoded.get_key_id();
            if (!jwks.has_jwk(kid)) {
                blockRequest(401, "Unauthorized",
                             R"({"error":"unauthorized","message":"unknown key id"})");
                return;
            }
            const auto& jwk = jwks.get_jwk(kid);
            if (!jwk.has_x5c()) {
                throw std::runtime_error("JWKS key missing x5c certificate");
            }
            pubKeyPem = jwt::helper::convert_base64_der_to_pem(
                jwk.get_x5c_key_value());
        } else {
            auto it = jwks.begin();
            if (it == jwks.end()) {
                blockRequest(401, "Unauthorized",
                             R"({"error":"unauthorized","message":"empty JWKS"})");
                return;
            }
            if (!it->has_x5c()) {
                throw std::runtime_error("JWKS key missing x5c certificate");
            }
            pubKeyPem = jwt::helper::convert_base64_der_to_pem(
                it->get_x5c_key_value());
        }

        auto verifier =
            jwt::verify<jwt::default_clock, JwtTraits>(jwt::default_clock{})
                .allow_algorithm(jwt::algorithm::rs256(pubKeyPem));

        if (!cfg.issuer.empty()) {
            verifier = verifier.with_issuer(cfg.issuer);
        }
        if (!cfg.audience.empty()) {
            verifier = verifier.with_audience(cfg.audience);
        }

        verifier.verify(decoded);

        auto& hdrs = msg->getHeaders();
        hdrs.set("x-jwt-subject", decoded.get_subject());
        if (decoded.has_issuer()) {
            hdrs.set("x-jwt-issuer", decoded.get_issuer());
        }
        hdrs.set("x-auth-method", "jwt");
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
