#include "server/admin_server.h"
#include "handler/admin_handler_factory.h"

#include <thread>

#include <folly/SocketAddress.h>
#include <glog/logging.h>
#include <proxygen/httpserver/HTTPServer.h>

namespace kimchi {

AdminServer::AdminServer(uint16_t port,
                         std::filesystem::path configDir,
                         std::shared_ptr<SharedConfig> sharedConfig) {
    proxygen::HTTPServerOptions opts;
    opts.threads = 1;
    opts.idleTimeout = std::chrono::milliseconds(60'000);
    opts.shutdownOn = {}; // data plane owns signal handling
    opts.handlerFactories.emplace_back(
        std::make_unique<AdminHandlerFactory>(std::move(sharedConfig),
                                             std::move(configDir)));

    folly::SocketAddress addr("0.0.0.0", port);
    server_ = std::make_unique<proxygen::HTTPServer>(std::move(opts));
    server_->bind({{addr, proxygen::HTTPServer::Protocol::HTTP}});
    LOG(INFO) << "Binding admin API on 0.0.0.0:" << port;
}

void AdminServer::start(std::function<void()> onReady) {
    serverThread_ = std::thread([this, onReady = std::move(onReady)]() mutable {
        server_->start(std::move(onReady),
                       [](std::exception_ptr ex) {
                           try {
                               if (ex) std::rethrow_exception(ex);
                           } catch (const std::exception& e) {
                               LOG(ERROR) << "Admin server error: " << e.what();
                           }
                       });
    });
}

void AdminServer::stop() {
    server_->stop();
}

void AdminServer::join() {
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

} // namespace kimchi
