#pragma once

#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

namespace piguard::infra {

class EventBus {
public:
    using Handler = std::function<void(const core::Event&)>;

    void subscribe(core::EventType type, Handler handler);
    void publish(const core::Event& event) const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<core::EventType, std::vector<Handler>> handlers_;
};

}  // namespace piguard::infra
