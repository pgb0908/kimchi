#include "config/loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <folly/json.h>
#include <glog/logging.h>

namespace kimchi::config {

// ── models fromJson implementations ──────────────────────────────────────────

static std::string getStr(const folly::dynamic& d, const char* key,
                          const std::string& def = "") {
    auto* p = d.get_ptr(key);
    return (p && p->isString()) ? p->getString() : def;
}

static int64_t getInt(const folly::dynamic& d, const char* key, int64_t def = 0) {
    auto* p = d.get_ptr(key);
    return (p && p->isInt()) ? p->getInt() : def;
}

static bool getBool(const folly::dynamic& d, const char* key, bool def = false) {
    auto* p = d.get_ptr(key);
    return (p && p->isBool()) ? p->getBool() : def;
}

Metadata Metadata::fromJson(const folly::dynamic& d) {
    Metadata m;
    m.name = getStr(d, "name");
    return m;
}

ListenerConfig ListenerConfig::fromJson(const folly::dynamic& d) {
    ListenerConfig cfg;
    cfg.apiVersion = getStr(d, "apiVersion");
    cfg.kind = getStr(d, "kind");

    if (auto* meta = d.get_ptr("metadata")) {
        cfg.metadata = Metadata::fromJson(*meta);
    }

    const auto& spec = d["spec"];
    cfg.protocol = getStr(spec, "protocol", "HTTP");
    cfg.port = static_cast<uint16_t>(getInt(spec, "port", 18000));
    cfg.host = getStr(spec, "host", "0.0.0.0");

    if (auto* ah = spec.get_ptr("allowedHostnames")) {
        for (const auto& h : *ah) {
            cfg.allowedHostnames.push_back(h.getString());
        }
    }

    if (auto* tlsNode = spec.get_ptr("tls")) {
        TlsConfig tls;
        tls.mode = getStr(*tlsNode, "mode", "TERMINATE");
        tls.minVersion = getStr(*tlsNode, "minVersion", "1.2");
        if (auto* certs = tlsNode->get_ptr("certificates")) {
            for (const auto& c : *certs) {
                TlsCertificate cert;
                cert.certRef = getStr(c, "certRef");
                cert.keyRef = getStr(c, "keyRef");
                tls.certificates.push_back(std::move(cert));
            }
        }
        cfg.tls = std::move(tls);
    }

    if (auto* conn = spec.get_ptr("connection")) {
        ConnectionConfig cc;
        cc.readTimeout = getStr(*conn, "readTimeout", "30s");
        cc.writeTimeout = getStr(*conn, "writeTimeout", "30s");
        cc.maxConnections = static_cast<int32_t>(getInt(*conn, "maxConnections", 0));
        cfg.connection = std::move(cc);
    }

    return cfg;
}

ServiceConfig ServiceConfig::fromJson(const folly::dynamic& d) {
    ServiceConfig cfg;
    cfg.apiVersion = getStr(d, "apiVersion");
    cfg.kind = getStr(d, "kind");

    if (auto* meta = d.get_ptr("metadata")) {
        cfg.metadata = Metadata::fromJson(*meta);
    }

    const auto& spec = d["spec"];
    cfg.protocol = getStr(spec, "protocol", "HTTP");

    if (auto* lb = spec.get_ptr("loadBalancing")) {
        cfg.loadBalancing.algorithm = getStr(*lb, "algorithm", "ROUND_ROBIN");
        if (auto* targets = lb->get_ptr("targets")) {
            for (const auto& t : *targets) {
                LoadBalancingTarget tgt;
                tgt.host = getStr(t, "host");
                tgt.port = static_cast<uint16_t>(getInt(t, "port", 80));
                tgt.weight = static_cast<int32_t>(getInt(t, "weight", 1));
                cfg.loadBalancing.targets.push_back(std::move(tgt));
            }
        }
    }

    return cfg;
}

RouterConfig RouterConfig::fromJson(const folly::dynamic& d) {
    RouterConfig cfg;
    cfg.apiVersion = getStr(d, "apiVersion");
    cfg.kind = getStr(d, "kind");

    if (auto* meta = d.get_ptr("metadata")) {
        cfg.metadata = Metadata::fromJson(*meta);
    }

    const auto& spec = d["spec"];

    if (auto* ref = spec.get_ptr("targetRef")) {
        cfg.targetRefName = getStr(*ref, "name");
    }

    if (auto* rules = spec.get_ptr("rules")) {
        for (const auto& r : *rules) {
            RouterRule rule;
            rule.path = getStr(r, "path");
            if (auto* pt = r.get_ptr("pathType")) {
                const auto ptStr = pt->getString();
                if (ptStr == "Exact")      rule.pathType = PathType::Exact;
                else if (ptStr == "Regex") rule.pathType = PathType::Regex;
                else                       rule.pathType = PathType::Prefix;
            }
            if (auto* methods = r.get_ptr("methods")) {
                for (const auto& m : *methods) {
                    rule.methods.push_back(m.getString());
                }
            }
            cfg.rules.push_back(std::move(rule));
        }
    }

    if (auto* confNode = spec.get_ptr("config")) {
        if (auto* dests = confNode->get_ptr("destinations")) {
            for (const auto& dest : *dests) {
                RouterDestination rd;
                if (auto* ref = dest.get_ptr("destinationRef")) {
                    rd.destinationKind = getStr(*ref, "kind", "Service");
                    rd.destinationName = getStr(*ref, "name");
                }
                rd.weight = static_cast<int32_t>(getInt(dest, "weight", 100));
                if (auto* rw = dest.get_ptr("rewrite")) {
                    RewriteConfig rwCfg;
                    rwCfg.path = getStr(*rw, "path");
                    rwCfg.host = getStr(*rw, "host");
                    rd.rewrite = std::move(rwCfg);
                }
                cfg.destinations.push_back(std::move(rd));
            }
        }
    }

    if (auto* pols = spec.get_ptr("policies")) {
        for (const auto& p : *pols) {
            cfg.policies.push_back(p.getString());
        }
    }

    return cfg;
}

PolicyConfig PolicyConfig::fromJson(const folly::dynamic& d) {
    PolicyConfig cfg;
    cfg.apiVersion = getStr(d, "apiVersion");
    cfg.kind = getStr(d, "kind");

    if (auto* meta = d.get_ptr("metadata")) {
        cfg.metadata = Metadata::fromJson(*meta);
    }

    const auto& spec = d["spec"];
    cfg.spec.order = static_cast<int32_t>(getInt(spec, "order", 0));

    if (auto* sec = spec.get_ptr("security")) {
        SecurityPolicySpec secSpec;
        if (auto* jwt = sec->get_ptr("jwt")) {
            JwtConfig jwtCfg;
            jwtCfg.jwksUri = getStr(*jwt, "jwksUri");
            jwtCfg.issuer = getStr(*jwt, "issuer");
            jwtCfg.audience = getStr(*jwt, "audience");
            jwtCfg.cacheTtlSeconds =
                static_cast<int32_t>(getInt(*jwt, "cacheTtlSeconds", 300));
            secSpec.jwt = std::move(jwtCfg);
        }
        cfg.spec.security = std::move(secSpec);
    }

    return cfg;
}

GatewayConfig GatewayConfig::fromJson(const folly::dynamic& d) {
    GatewayConfig cfg;
    cfg.apiVersion = getStr(d, "apiVersion");
    cfg.kind = getStr(d, "kind");

    if (auto* meta = d.get_ptr("metadata")) {
        cfg.metadata = Metadata::fromJson(*meta);
    }

    const auto& spec = d["spec"];

    if (auto* obs = spec.get_ptr("observability")) {
        if (auto* al = obs->get_ptr("accessLog")) {
            cfg.accessLog.enabled = getBool(*al, "enabled", true);
            cfg.accessLog.format = getStr(*al, "format", "JSON");
        }
    }

    if (auto* srv = spec.get_ptr("server")) {
        cfg.server.workerThreads =
            static_cast<int32_t>(getInt(*srv, "workerThreads", 0));
        cfg.server.idleTimeoutMs =
            static_cast<int32_t>(getInt(*srv, "idleTimeoutMs", 60000));
        cfg.server.listenBacklog =
            static_cast<int32_t>(getInt(*srv, "listenBacklog", 1024));
        cfg.server.maxConcurrentStreams =
            static_cast<int32_t>(getInt(*srv, "maxConcurrentStreams", 100));
        cfg.server.initialReceiveWindowBytes =
            static_cast<int32_t>(getInt(*srv, "initialReceiveWindowBytes", 65536));
        cfg.server.useZeroCopy = getBool(*srv, "useZeroCopy", false);
    }

    return cfg;
}

// ── ConfigLoader ──────────────────────────────────────────────────────────────

void ConfigLoader::dispatchDocument(const folly::dynamic& doc, ConfigStore& store) {
    auto* kindPtr = doc.get_ptr("kind");
    if (!kindPtr || !kindPtr->isString()) {
        LOG(WARNING) << "Config document missing 'kind' field, skipping";
        return;
    }

    const std::string kind = kindPtr->getString();

    if (kind == "Listener") {
        store.listeners.push_back(ListenerConfig::fromJson(doc));
        LOG(INFO) << "Loaded Listener: " << store.listeners.back().metadata.name;
    } else if (kind == "Router") {
        store.routers.push_back(RouterConfig::fromJson(doc));
        LOG(INFO) << "Loaded Router: " << store.routers.back().metadata.name;
    } else if (kind == "Service") {
        store.services.push_back(ServiceConfig::fromJson(doc));
        LOG(INFO) << "Loaded Service: " << store.services.back().metadata.name;
    } else if (kind == "Gateway") {
        store.gateway = GatewayConfig::fromJson(doc);
        LOG(INFO) << "Loaded Gateway: " << store.gateway->metadata.name;
    } else if (kind == "Policy") {
        store.policies.push_back(PolicyConfig::fromJson(doc));
        LOG(INFO) << "Loaded Policy: " << store.policies.back().metadata.name;
    } else if (kind == "MockingService") {
        LOG(WARNING) << "MockingService kind not yet implemented, skipping";
    } else {
        LOG(WARNING) << "Unknown config kind '" << kind << "', skipping";
    }
}

ConfigStore ConfigLoader::loadFromDirectory(const std::filesystem::path& configDir) {
    ConfigStore store;

    if (!std::filesystem::exists(configDir)) {
        LOG(WARNING) << "Config directory does not exist: " << configDir;
        return store;
    }

    for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        const auto& path = entry.path();
        std::ifstream file(path);
        if (!file) {
            LOG(ERROR) << "Cannot open config file: " << path;
            continue;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        const std::string content = ss.str();

        try {
            folly::dynamic doc = folly::parseJson(content);
            dispatchDocument(doc, store);
        } catch (const std::exception& ex) {
            LOG(ERROR) << "Failed to parse " << path << ": " << ex.what();
        }
    }

    return store;
}

void ConfigLoader::applyJsonDocument(const std::string& jsonText, ConfigStore& store) {
    folly::dynamic doc = folly::parseJson(jsonText);
    dispatchDocument(doc, store);
}

} // namespace kimchi::config
