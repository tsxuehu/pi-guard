#pragma once

#include <string>

#include "foundation/module.hpp"

namespace piguard::capture_video {

class VideoCaptureModule : public foundation::Module {
public:
    explicit VideoCaptureModule();

    std::string name() const override;
    bool start() override;
    void stop() override;

private:

};

}  // namespace piguard::capture_video
