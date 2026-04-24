#include "external_access/websocket/websocket_client.hpp"

namespace piguard::external_access {

std::string WebSocketClient::name() const { return "WebSocketClient"; }

bool WebSocketClient::start() {
    running_.store(true);
    return true;
}

void WebSocketClient::stop() { running_.store(false); }

void WebSocketClient::poll_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::external_access
