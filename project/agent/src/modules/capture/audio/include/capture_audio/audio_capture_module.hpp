#pragma once

#include <string>

#include "foundation/module.hpp"
#include "foundation/thread_safe_queue.hpp"
#include "foundation/types.hpp"

namespace piguard::capture {

class AudioCapture : public foundation::Module {
public:
    explicit AudioCapture(foundation::ThreadSafeQueue<foundation::AudioFrame>& out_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;

private:
    foundation::ThreadSafeQueue<foundation::AudioFrame>& out_queue_;
};

}  // namespace piguard::capture
