#pragma once

#include <atomic>
#include <string>

#include "core/module.hpp"
#include "core/thread_safe_queue.hpp"
#include "core/types.hpp"

namespace piguard::capture {

class VideoCapture : public core::Module {
public:
    explicit VideoCapture(core::ThreadSafeQueue<core::VideoFrame>& out_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void poll_once();

private:
    core::ThreadSafeQueue<core::VideoFrame>& out_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::capture
