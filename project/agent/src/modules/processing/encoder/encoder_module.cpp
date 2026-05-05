#include "processing_encoder/encoder_module.hpp"

namespace piguard::processing_encoder {

EncoderModule::EncoderModule() = default;

EncoderModule::~EncoderModule() {
    stop();
}

std::string EncoderModule::name() const {
    return "EncoderModule";
}

bool EncoderModule::start() {
    return true;
}

void EncoderModule::stop() {
}

}  // namespace piguard::processing_encoder
