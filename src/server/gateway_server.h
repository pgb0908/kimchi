#pragma once

#include "config/models.h"
#include "policy/jwks_cache.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <proxygen/httpserver/HTTPServer.h>

namespace kimchi {

class GatewayServer {
public:
    GatewayServer(const std::vector<config::ListenerConfig>& listeners,
                  std::shared_ptr<const config::ConfigStore> store,
                  std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches = {});

    void start(std::function<void()> onReady = nullptr);
    void stop();
    void join();

private:
    std::unique_ptr<proxygen::HTTPServer> server_;
    std::thread serverThread_;
};

} // namespace kimchi
