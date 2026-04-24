#pragma once

#include <atomic>
#include <string>

#include "module.hpp"
#include "thread_safe_queue.hpp"
#include "types.hpp"

namespace piguard::capture {

class AudioCapture : public foundation::Module {
public:
    explicit AudioCapture(foundation::ThreadSafeQueue<foundation::AudioFrame>& out_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void poll_once();

private:
    foundation::ThreadSafeQueue<foundation::AudioFrame>& out_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::capture
