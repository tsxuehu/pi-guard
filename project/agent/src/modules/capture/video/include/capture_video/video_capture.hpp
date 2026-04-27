#pragma once

#include <string>

#include "foundation/module.hpp"

namespace piguard::capture_video {

class VideoCapture : public foundation::Module {
public:
    explicit VideoCapture();

    std::string name() const override;
    bool start() override;
    void stop() override;

private:
    
};

}  // namespace piguard::capture_video
