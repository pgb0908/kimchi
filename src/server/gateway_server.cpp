#include "server/gateway_server.h"
#include "handler/gateway_handler_factory.h"

#include <map>
#include <thread>

#include <folly/SocketAddress.h>
#include <glog/logging.h>
#include <proxygen/httpserver/HTTPServer.h>

namespace kimchi {

GatewayServer::GatewayServer(const std::vector<config::ListenerConfig>& listeners,
                             std::shared_ptr<const config::ConfigStore> store,
                             std::map<std::string, std::shared_ptr<JwksCache>> jwksCaches) {
    const config::ServerTuningConfig& tuning = store->gateway
        ? store->gateway->server
        : config::ServerTuningConfig{};

    proxygen::HTTPServerOptions opts;
    opts.threads = tuning.workerThreads > 0
        ? static_cast<size_t>(tuning.workerThreads)
        : std::thread::hardware_concurrency();
    opts.idleTimeout        = std::chrono::milliseconds(tuning.idleTimeoutMs);
    opts.listenBacklog      = static_cast<uint32_t>(tuning.listenBacklog);
    opts.maxConcurrentIncomingStreams =
        static_cast<uint32_t>(tuning.maxConcurrentStreams);
    opts.initialReceiveWindow    = static_cast<size_t>(tuning.initialReceiveWindowBytes);
    opts.receiveStreamWindowSize = static_cast<size_t>(tuning.initialReceiveWindowBytes);
    opts.receiveSessionWindowSize= static_cast<size_t>(tuning.initialReceiveWindowBytes);
    opts.useZeroCopy        = tuning.useZeroCopy;
    opts.shutdownOn         = {SIGINT, SIGTERM};

    LOG(INFO) << "Server tuning — threads:" << opts.threads
              << " idleTimeout:" << tuning.idleTimeoutMs << "ms"
              << " listenBacklog:" << tuning.listenBacklog
              << " zeroCopy:" << (tuning.useZeroCopy ? "on" : "off");
    opts.handlerFactories.emplace_back(
        std::make_unique<GatewayHandlerFactory>(std::move(store),
                                                std::move(jwksCaches)));

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
