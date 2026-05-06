#pragma once

#include "config/models.h"

#include <filesystem>
#include <memory>
#include <mutex>

#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/lib/http/HTTPMessage.h>

namespace kimchi {

struct SharedConfig {
    std::mutex mu;
    std::shared_ptr<const config::ConfigStore> current;
};

class AdminHandler : public proxygen::RequestHandler {
public:
    AdminHandler(std::shared_ptr<SharedConfig> sharedConfig,
                 std::filesystem::path configDir);

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;
    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;
    void onEOM() noexcept override;
    void onUpgrade(proxygen::UpgradeProtocol prot) noexcept override;
    void requestComplete() noexcept override;
    void onError(proxygen::ProxygenError err) noexcept override;

private:
    std::shared_ptr<SharedConfig> sharedConfig_;
    std::filesystem::path configDir_;
    std::unique_ptr<proxygen::HTTPMessage> requestHeaders_;
};

} // namespace kimchi
