#pragma once

#include "config/models.h"

#include <memory>
#include <regex>

#include <folly/io/IOBuf.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/lib/http/HTTPMessage.h>

namespace kimchi {

class GatewayHandler : public proxygen::RequestHandler {
public:
    explicit GatewayHandler(std::shared_ptr<const config::ConfigStore> store);

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;
    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;
    void onEOM() noexcept override;
    void onUpgrade(proxygen::UpgradeProtocol prot) noexcept override;
    void requestComplete() noexcept override;
    void onError(proxygen::ProxygenError err) noexcept override;

private:
    std::shared_ptr<const config::ConfigStore> store_;
    std::unique_ptr<proxygen::HTTPMessage> requestHeaders_;
    std::unique_ptr<folly::IOBuf> requestBody_;
};

} // namespace kimchi
