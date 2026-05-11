#include "handler/upstream_client.h"

#include <glog/logging.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/http/session/HTTPUpstreamSession.h>

namespace kimchi {

UpstreamClient::UpstreamClient(proxygen::ResponseHandler* downstream,
                               folly::EventBase* evb,
                               std::unique_ptr<proxygen::HTTPMessage> req)
    : downstream_(downstream),
      evb_(evb),
      timer_(std::chrono::milliseconds(30000), evb),
      connector_(this, timer_),
      upstreamReq_(std::move(req)) {}

void UpstreamClient::connect(const folly::SocketAddress& addr) {
    connector_.connect(evb_, addr, std::chrono::milliseconds(5000));
}

void UpstreamClient::connectSuccess(
    proxygen::HTTPUpstreamSession* session) noexcept {
    txn_ = session->newTransaction(this);
    txn_->sendHeaders(*upstreamReq_);
    for (auto& buf : pendingBodies_) {
        txn_->sendBody(std::move(buf));
    }
    pendingBodies_.clear();
    connected_ = true;
    if (eomReceived_) {
        txn_->sendEOM();
    }
}

void UpstreamClient::sendBody(std::unique_ptr<folly::IOBuf> body) {
    if (connected_) {
        txn_->sendBody(std::move(body));
    } else {
        pendingBodies_.push_back(std::move(body));
    }
}

void UpstreamClient::sendEOM() {
    if (connected_) {
        txn_->sendEOM();
    } else {
        eomReceived_ = true;
    }
}

void UpstreamClient::connectError(
    const folly::AsyncSocketException& ex) noexcept {
    LOG(ERROR) << "upstream connect failed: " << ex.what();
    sendErrorResponse(502, R"({"error":"bad_gateway","message":"upstream connect failed"})");
    delete this;
}

void UpstreamClient::setTransaction(
    proxygen::HTTPTransaction* txn) noexcept {
    txn_ = txn;
}

void UpstreamClient::detachTransaction() noexcept {
    delete this;
}

void UpstreamClient::onHeadersComplete(
    std::unique_ptr<proxygen::HTTPMessage> msg) noexcept {
    headersSent_ = true;
    downstream_->sendHeaders(*msg);
}

void UpstreamClient::onBody(std::unique_ptr<folly::IOBuf> chain) noexcept {
    downstream_->sendBody(std::move(chain));
}

void UpstreamClient::onTrailers(
    std::unique_ptr<proxygen::HTTPHeaders> /*trailers*/) noexcept {}

void UpstreamClient::onEOM() noexcept {
    downstream_->sendEOM();
}

void UpstreamClient::onUpgrade(
    proxygen::UpgradeProtocol /*protocol*/) noexcept {}

void UpstreamClient::onEgressPaused() noexcept {}

void UpstreamClient::onEgressResumed() noexcept {}

void UpstreamClient::onError(
    const proxygen::HTTPException& error) noexcept {
    LOG(ERROR) << "upstream error: " << error.what();
    if (!headersSent_) {
        sendErrorResponse(502, R"({"error":"bad_gateway","message":"upstream error"})");
    }
}

void UpstreamClient::sendErrorResponse(uint16_t status,
                                       const std::string& body) {
    proxygen::ResponseBuilder(downstream_)
        .status(status, status == 502 ? "Bad Gateway" : "Service Unavailable")
        .header("Content-Type", "application/json")
        .body(body)
        .sendWithEOM();
}

} // namespace kimchi
