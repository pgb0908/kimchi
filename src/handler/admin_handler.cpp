#include "handler/admin_handler.h"
#include "config/loader.h"

#include <glog/logging.h>
#include <proxygen/httpserver/ResponseBuilder.h>

namespace kimchi {

AdminHandler::AdminHandler(std::shared_ptr<SharedConfig> sharedConfig,
                           std::filesystem::path configDir)
    : sharedConfig_(std::move(sharedConfig)),
      configDir_(std::move(configDir)) {}

void AdminHandler::onRequest(
    std::unique_ptr<proxygen::HTTPMessage> headers) noexcept {
    requestHeaders_ = std::move(headers);
}

void AdminHandler::onBody(std::unique_ptr<folly::IOBuf> /*body*/) noexcept {}

void AdminHandler::onEOM() noexcept {
    const std::string path = requestHeaders_->getPath();
    const auto method = requestHeaders_->getMethod();

    if (path == "/healthz" && method == proxygen::HTTPMethod::GET) {
        proxygen::ResponseBuilder(downstream_)
            .status(200, "OK")
            .header("Content-Type", "application/json")
            .body("{\"status\":\"healthy\"}")
            .sendWithEOM();
        return;
    }

    if (path == "/config/reload" && method == proxygen::HTTPMethod::POST) {
        try {
            auto newStore = config::ConfigLoader::loadFromDirectory(configDir_);
            {
                std::lock_guard<std::mutex> lock(sharedConfig_->mu);
                sharedConfig_->current =
                    std::make_shared<config::ConfigStore>(std::move(newStore));
            }
            LOG(INFO) << "Config reloaded from " << configDir_;
            proxygen::ResponseBuilder(downstream_)
                .status(200, "OK")
                .header("Content-Type", "application/json")
                .body("{\"reloaded\":true}")
                .sendWithEOM();
        } catch (const std::exception& ex) {
            LOG(ERROR) << "Config reload failed: " << ex.what();
            proxygen::ResponseBuilder(downstream_)
                .status(500, "Internal Server Error")
                .header("Content-Type", "application/json")
                .body(std::string("{\"error\":\"") + ex.what() + "\"}")
                .sendWithEOM();
        }
        return;
    }

    proxygen::ResponseBuilder(downstream_)
        .status(404, "Not Found")
        .header("Content-Type", "application/json")
        .body("{\"error\":\"not found\"}")
        .sendWithEOM();
}

void AdminHandler::onUpgrade(proxygen::UpgradeProtocol /*prot*/) noexcept {
    proxygen::ResponseBuilder(downstream_)
        .status(400, "Bad Request")
        .sendWithEOM();
}

void AdminHandler::requestComplete() noexcept {
    delete this;
}

void AdminHandler::onError(proxygen::ProxygenError /*err*/) noexcept {
    delete this;
}

} // namespace kimchi
