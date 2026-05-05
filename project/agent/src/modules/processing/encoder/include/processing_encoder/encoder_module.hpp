#pragma once

#include <memory>
#include <string>

#include "foundation/module.hpp"
namespace piguard::processing_encoder {

class EncoderModule : public foundation::Module {
public:
    EncoderModule();
    ~EncoderModule() override;

    std::string name() const override;
    bool start() override;
    void stop() override;

};

}  // namespace piguard::processing_encoder
