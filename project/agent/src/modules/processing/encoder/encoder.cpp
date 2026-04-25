#include "processing_encoder/encoder.hpp"

namespace piguard::processing {

std::string Encoder::name() const { return "Encoder"; }

bool Encoder::start() {
    running_.store(true);
    return true;
}

void Encoder::stop() { running_.store(false); }

void Encoder::encode_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::processing
