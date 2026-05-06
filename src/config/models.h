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

struct RouterRule {
    std::string path;
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

    static RouterConfig fromJson(const folly::dynamic& d);
};

// ── Gateway ───────────────────────────────────────────────────────────────────

struct AccessLogConfig {
    bool enabled = true;
    std::string format = "JSON";
};

struct GatewayConfig {
    std::string apiVersion;
    std::string kind;
    Metadata metadata;

    AccessLogConfig accessLog;

    static GatewayConfig fromJson(const folly::dynamic& d);
};

// ── Aggregate ─────────────────────────────────────────────────────────────────

struct ConfigStore {
    std::optional<GatewayConfig> gateway;
    std::vector<ListenerConfig> listeners;
    std::vector<RouterConfig> routers;
    std::vector<ServiceConfig> services;
};

} // namespace kimchi::config
