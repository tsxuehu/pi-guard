#pragma once

#include <string>

namespace piguard::core {

class Module {
public:
    virtual ~Module() = default;

    virtual std::string name() const = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
};

}  // namespace piguard::core
