#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <folly/dynamic.h>

namespace kimchi::config {

struct Metadata {
    std::string name;

    static Metadata fromJson(const folly::dynamic& d);
};

// ── Listener ──────────────────────────────────────────────────────────────────

struct TlsCertificate {
    std::string certRef;
    std::string keyRef;
};

struct TlsConfig {
    std::string mode;       // TERMINATE | PASSTHROUGH
    std::string minVersion; // "1.2" | "1.3"
    std::vector<TlsCertificate> certificates;
};

struct ConnectionConfig {
    std::string readTimeout;
    std::string writeTimeout;
    int32_t maxConnections = 0;
};

struct ListenerConfig {
    std::string apiVersion;
    std::string kind;
    Metadata metadata;

    std::string protocol; // HTTP | HTTPS | TCP | GRPC
    uint16_t port = 0;
    std::string host = "0.0.0.0";
    std::vector<std::string> allowedHostnames;
    std::optional<TlsConfig> tls;
    std::optional<ConnectionConfig> connection;

    static ListenerConfig fromJson(const folly::dynamic& d);
};

// ── Service ───────────────────────────────────────────────────────────────────

struct LoadBalancingTarget {
    std::string host;
    uint16_t port = 0;
    int32_t weight = 1;
};

struct LoadBalancingConfig {
    std::string algorithm = "ROUND_ROBIN";
    std::vector<LoadBalancingTarget> targets;
};

struct ServiceConfig {
    std::string apiVersion;
    std::string kind;
    Metadata metadata;

    std::string protocol = "HTTP";
    LoadBalancingConfig loadBalancing;

    static ServiceConfig fromJson(const folly::dynamic& d);
};

// ── Router ────────────────────────────────────────────────────────────────────

enum class PathType { Exact, Prefix, Regex };

struct RouterRule {
    std::string path;
    PathType pathType = PathType::Prefix;
    std::vector<std::string> methods;
};

struct RewriteConfig {
    std::string path;
    std::string host;
};

struct RouterDestination {
    std::string destinationKind = "Service";
    std::string destinationName;
    int32_t weight = 100;
    std::optional<RewriteConfig> rewrite;
};

struct RouterConfig {
    std::string apiVersion;
    std::string kind;
    Metadata metadata;

    std::string targetRefName;
    std::vector<RouterRule> rules;
    std::vector<RouterDestination> destinations;
    std::vector<std::string> policies;  // policy resource names (ordered)

    static RouterConfig fromJson(const folly::dynamic& d);
};

// ── Gateway ───────────────────────────────────────────────────────────────────

struct AccessLogConfig {
    bool enabled = true;
    std::string format = "JSON";
};

struct ServerTuningConfig {
    // IO worker thread count. 0 = hardware_concurrency().
    int32_t workerThreads = 0;
    // Idle connection / slow-request timeout (ms).
    int32_t idleTimeoutMs = 60000;
    // TCP accept queue depth.
    int32_t listenBacklog = 1024;
    // Max concurrent HTTP/2 streams per connection.
    int32_t maxConcurrentStreams = 100;
    // HTTP/2 initial receive window size (bytes).
    int32_t initialReceiveWindowBytes = 65536;
    // Enable SO_ZEROCOPY for zero-copy socket sends.
    bool useZeroCopy = false;
};

struct GatewayConfig {
    std::string apiVersion;
    std::string kind;
    Metadata metadata;

    AccessLogConfig accessLog;
    ServerTuningConfig server;

    static GatewayConfig fromJson(const folly::dynamic& d);
};

// ── Policy ────────────────────────────────────────────────────────────────────

struct JwtConfig {
    std::string jwksUri;
    std::string issuer;
    std::string audience;
    int32_t cacheTtlSeconds = 300;
};

struct SecurityPolicySpec {
    std::optional<JwtConfig> jwt;
};

struct PolicySpec {
    int32_t order = 0;
    std::optional<SecurityPolicySpec> security;
};

struct PolicyConfig {
    std::string apiVersion;
    std::string kind;
    Metadata metadata;
    PolicySpec spec;

    static PolicyConfig fromJson(const folly::dynamic& d);
};

// ── Aggregate ─────────────────────────────────────────────────────────────────

struct ConfigStore {
    std::optional<GatewayConfig> gateway;
    std::vector<ListenerConfig> listeners;
    std::vector<RouterConfig> routers;
    std::vector<ServiceConfig> services;
    std::vector<PolicyConfig> policies;
};

} // namespace kimchi::config
