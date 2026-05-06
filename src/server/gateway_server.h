#pragma once

#include "config/models.h"

#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <proxygen/httpserver/HTTPServer.h>

namespace kimchi {

class GatewayServer {
public:
    GatewayServer(const std::vector<config::ListenerConfig>& listeners,
                  std::shared_ptr<const config::ConfigStore> store);

    void start(std::function<void()> onReady = nullptr);
    void stop();
    void join();

private:
    std::unique_ptr<proxygen::HTTPServer> server_;
    std::thread serverThread_;
};

} // namespace kimchi
