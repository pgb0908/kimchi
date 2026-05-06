#include "server/gateway_server.h"
#include "handler/gateway_handler_factory.h"

#include <thread>

#include <folly/SocketAddress.h>
#include <glog/logging.h>
#include <proxygen/httpserver/HTTPServer.h>

namespace kimchi {

GatewayServer::GatewayServer(const std::vector<config::ListenerConfig>& listeners,
                             std::shared_ptr<const config::ConfigStore> store) {
    proxygen::HTTPServerOptions opts;
    opts.threads = std::thread::hardware_concurrency();
    opts.idleTimeout = std::chrono::milliseconds(60'000);
    opts.shutdownOn = {SIGINT, SIGTERM};
    opts.handlerFactories.emplace_back(
        std::make_unique<GatewayHandlerFactory>(std::move(store)));

    std::vector<proxygen::HTTPServer::IPConfig> ipConfigs;
    for (const auto& listener : listeners) {
        if (listener.tls.has_value()) {
            LOG(WARNING) << "TLS not yet supported in skeleton; "
                         << "listener '" << listener.metadata.name
                         << "' will use plain HTTP";
        }

        folly::SocketAddress addr(listener.host, listener.port);
        ipConfigs.push_back({addr, proxygen::HTTPServer::Protocol::HTTP});
        LOG(INFO) << "Binding data plane on " << listener.host << ":"
                  << listener.port;
    }

    server_ = std::make_unique<proxygen::HTTPServer>(std::move(opts));
    server_->bind(ipConfigs);
}

void GatewayServer::start(std::function<void()> onReady) {
    serverThread_ = std::thread([this, onReady = std::move(onReady)]() mutable {
        server_->start(std::move(onReady),
                       [](std::exception_ptr ex) {
                           try {
                               if (ex) std::rethrow_exception(ex);
                           } catch (const std::exception& e) {
                               LOG(ERROR) << "Gateway server error: " << e.what();
                           }
                       });
    });
}

void GatewayServer::stop() {
    server_->stop();
}

void GatewayServer::join() {
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

} // namespace kimchi
