#include "handler/admin_handler_factory.h"

namespace kimchi {

AdminHandlerFactory::AdminHandlerFactory(std::shared_ptr<SharedConfig> sharedConfig,
                                         std::filesystem::path configDir)
    : sharedConfig_(std::move(sharedConfig)),
      configDir_(std::move(configDir)) {}

void AdminHandlerFactory::onServerStart(folly::EventBase* /*evb*/) noexcept {}

void AdminHandlerFactory::onServerStop() noexcept {}

proxygen::RequestHandler* AdminHandlerFactory::onRequest(
    proxygen::RequestHandler* /*upstream*/,
    proxygen::HTTPMessage* /*msg*/) noexcept {
    return new AdminHandler(sharedConfig_, configDir_);
}

} // namespace kimchi
