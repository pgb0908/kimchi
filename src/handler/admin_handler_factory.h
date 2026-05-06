#pragma once

#include "handler/admin_handler.h"

#include <filesystem>
#include <memory>

#include <proxygen/httpserver/RequestHandlerFactory.h>

namespace kimchi {

class AdminHandlerFactory : public proxygen::RequestHandlerFactory {
public:
    AdminHandlerFactory(std::shared_ptr<SharedConfig> sharedConfig,
                        std::filesystem::path configDir);

    void onServerStart(folly::EventBase* evb) noexcept override;
    void onServerStop() noexcept override;

    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler* upstream,
        proxygen::HTTPMessage* msg) noexcept override;

private:
    std::shared_ptr<SharedConfig> sharedConfig_;
    std::filesystem::path configDir_;
};

} // namespace kimchi
