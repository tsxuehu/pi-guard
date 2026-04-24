#include "infra/event_bus/event_bus.hpp"

#include <mutex>
#include <utility>

namespace piguard::infra {

void EventBus::subscribe(core::EventType type, Handler handler) {
    std::unique_lock lock(mutex_);
    handlers_[type].push_back(std::move(handler));
}

void EventBus::publish(const core::Event& event) const {
    std::shared_lock lock(mutex_);
    const auto it = handlers_.find(event.type);
    if (it == handlers_.end()) {
        return;
    }
    for (const auto& handler : it->second) {
        handler(event);
    }
}

}  // namespace piguard::infra
