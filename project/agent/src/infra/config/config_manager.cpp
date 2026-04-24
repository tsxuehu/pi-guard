#include "infra/config/config_manager.hpp"

#include <mutex>
#include <utility>

namespace piguard::infra {

ConfigManager::ConfigManager(std::string path) : path_(std::move(path)) {}

std::string ConfigManager::name() const { return "ConfigManager"; }

bool ConfigManager::start() { return reload(); }

void ConfigManager::stop() {}

bool ConfigManager::reload() {
    std::unique_lock lock(mutex_);
    kv_["config_path"] = path_;
    kv_["camera"] = "/dev/video0";
    kv_["mic_device"] = "hw:0";
    kv_["speaker_device"] = "hw:0";
    return true;
}

std::string ConfigManager::get_string(const std::string& key, const std::string& fallback) const {
    std::shared_lock lock(mutex_);
    const auto it = kv_.find(key);
    if (it == kv_.end()) {
        return fallback;
    }
    return it->second;
}

void ConfigManager::set_string(const std::string& key, std::string value) {
    std::unique_lock lock(mutex_);
    kv_[key] = std::move(value);
}

}  // namespace piguard::infra
