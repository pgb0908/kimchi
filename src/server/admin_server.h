#pragma once

#include "handler/admin_handler.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <thread>

#include <proxygen/httpserver/HTTPServer.h>

namespace kimchi {

class AdminServer {
public:
    AdminServer(uint16_t port,
                std::filesystem::path configDir,
                std::shared_ptr<SharedConfig> sharedConfig);

    void start(std::function<void()> onReady = nullptr);
    void stop();
    void join();

private:
    std::unique_ptr<proxygen::HTTPServer> server_;
    std::thread serverThread_;
};

} // namespace kimchi
