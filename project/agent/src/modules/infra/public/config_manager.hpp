#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "module.hpp"

namespace piguard::infra {

class ConfigManager : public foundation::Module {
public:
    explicit ConfigManager(std::string path);

    std::string name() const override;
    bool start() override;
    void stop() override;

    std::string get_string(const std::string& key, const std::string& fallback = "") const;
    void set_string(const std::string& key, std::string value);
    bool reload();

private:
    std::string path_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> kv_;
};

}  // namespace piguard::infra
