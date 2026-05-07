#include "policy/jwks_cache.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

#include <folly/Uri.h>
#include <glog/logging.h>

namespace kimchi {

namespace {

// Plain HTTP GET over POSIX sockets.
// HTTPS JWKS URIs are not supported in v1; use an HTTP sidecar or an
// internal IdP endpoint that speaks plain HTTP within the cluster.
std::string httpGet(const std::string& host, uint16_t port,
                    const std::string& path) {
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    int rc = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(),
                           &hints, &res);
    if (rc != 0) {
        throw std::runtime_error(std::string("JWKS DNS lookup failed: ") +
                                 ::gai_strerror(rc));
    }

    int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        ::freeaddrinfo(res);
        throw std::runtime_error("JWKS socket() failed");
    }

    struct timeval tv{10, 0};  // 10-second connect/recv timeout
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (::connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        ::freeaddrinfo(res);
        ::close(sock);
        throw std::runtime_error("JWKS connect() to " + host + ":" +
                                 std::to_string(port) + " failed");
    }
    ::freeaddrinfo(res);

    const std::string req = "GET " + path +
                            " HTTP/1.0\r\nHost: " + host +
                            "\r\nAccept: application/json\r\n"
                            "Connection: close\r\n\r\n";
    if (::send(sock, req.c_str(), req.size(), 0) < 0) {
        ::close(sock);
        throw std::runtime_error("JWKS send() failed");
    }

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = ::recv(sock, buf, sizeof(buf), 0)) > 0) {
        response.append(buf, static_cast<size_t>(n));
    }
    ::close(sock);

    // Extract HTTP status code
    if (response.size() < 12 || response.substr(0, 5) != "HTTP/") {
        throw std::runtime_error("JWKS: invalid HTTP response");
    }
    int statusCode = std::stoi(response.substr(9, 3));
    if (statusCode != 200) {
        throw std::runtime_error("JWKS endpoint returned HTTP " +
                                 std::to_string(statusCode));
    }

    auto pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) {
        throw std::runtime_error("JWKS: malformed HTTP response (no header end)");
    }
    return response.substr(pos + 4);
}

} // namespace

std::shared_ptr<JwksCache> JwksCache::fetch(const config::JwtConfig& cfg) {
    if (cfg.jwksUri.empty()) {
        throw std::runtime_error("JwtConfig.jwksUri is empty");
    }

    folly::Uri uri(cfg.jwksUri);

    if (uri.scheme() != "http") {
        throw std::runtime_error(
            "JWKS URI scheme '" + uri.scheme().toStdString() +
            "' is not supported (only 'http' in v1); got: " + cfg.jwksUri);
    }

    std::string host = uri.host().toStdString();
    uint16_t port = uri.port() ? static_cast<uint16_t>(uri.port()) : 80;
    std::string path = uri.path().toStdString();
    if (path.empty()) path = "/";

    const auto& qs = uri.query();
    if (!qs.empty()) {
        path += "?" + qs.toStdString();
    }

    LOG(INFO) << "Fetching JWKS from " << host << ":" << port << path;
    std::string json = httpGet(host, port, path);
    LOG(INFO) << "JWKS fetched successfully (" << json.size() << " bytes)";

    return std::shared_ptr<JwksCache>(new JwksCache(cfg, std::move(json)));
}

} // namespace kimchi
