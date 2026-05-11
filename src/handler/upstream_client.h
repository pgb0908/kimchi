#pragma once

#include <memory>
#include <vector>

#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <proxygen/httpserver/ResponseHandler.h>
#include <proxygen/lib/http/HTTPConnector.h>
#include <proxygen/lib/http/HTTPMessage.h>
#include <proxygen/lib/http/session/HTTPTransaction.h>
#include <proxygen/lib/http/session/HTTPUpstreamSession.h>
#include <proxygen/lib/utils/WheelTimerInstance.h>

namespace kimchi {

class UpstreamClient : public proxygen::HTTPConnector::Callback,
                       public proxygen::HTTPTransactionHandler {
public:
    UpstreamClient(proxygen::ResponseHandler* downstream,
                   folly::EventBase* evb,
                   std::unique_ptr<proxygen::HTTPMessage> req);

    void connect(const folly::SocketAddress& addr);
    void sendBody(std::unique_ptr<folly::IOBuf> body);
    void sendEOM();

    // HTTPConnector::Callback
    void connectSuccess(proxygen::HTTPUpstreamSession* session) noexcept override;
    void connectError(const folly::AsyncSocketException& ex) noexcept override;

    // HTTPTransactionHandler
    void setTransaction(proxygen::HTTPTransaction* txn) noexcept override;
    void detachTransaction() noexcept override;
    void onHeadersComplete(std::unique_ptr<proxygen::HTTPMessage> msg) noexcept override;
    void onBody(std::unique_ptr<folly::IOBuf> chain) noexcept override;
    void onTrailers(std::unique_ptr<proxygen::HTTPHeaders> trailers) noexcept override;
    void onEOM() noexcept override;
    void onUpgrade(proxygen::UpgradeProtocol protocol) noexcept override;
    void onError(const proxygen::HTTPException& error) noexcept override;
    void onEgressPaused() noexcept override;
    void onEgressResumed() noexcept override;

private:
    void sendErrorResponse(uint16_t status, const std::string& body);

    proxygen::ResponseHandler* downstream_;
    folly::EventBase* evb_;
    proxygen::WheelTimerInstance timer_;
    proxygen::HTTPConnector connector_;
    proxygen::HTTPTransaction* txn_{nullptr};
    std::unique_ptr<proxygen::HTTPMessage> upstreamReq_;
    std::vector<std::unique_ptr<folly::IOBuf>> pendingBodies_;
    bool connected_{false};
    bool eomReceived_{false};
    bool headersSent_{false};
};

} // namespace kimchi
