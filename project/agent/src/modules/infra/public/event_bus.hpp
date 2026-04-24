#pragma once

#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace piguard::infra {

class EventBus {
public:
    using Handler = std::function<void(const foundation::Event&)>;

    void subscribe(foundation::EventType type, Handler handler);
    void publish(const foundation::Event& event) const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<foundation::EventType, std::vector<Handler>> handlers_;
};

}  // namespace piguard::infra
