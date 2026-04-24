#include "external_access/http/http_notify.hpp"

namespace piguard::external_access {

std::string HTTPNotify::name() const { return "HTTPNotify"; }

bool HTTPNotify::start() {
    running_.store(true);
    return true;
}

void HTTPNotify::stop() { running_.store(false); }

void HTTPNotify::notify_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::external_access
