#pragma once

#include <atomic>
#include <string>

#include "core/module.hpp"
#include "core/thread_safe_queue.hpp"
#include "core/types.hpp"

namespace piguard::processing {

class MotionDetect : public core::Module {
public:
    MotionDetect(core::ThreadSafeQueue<core::VideoFrame>& in_queue, core::ThreadSafeQueue<core::Event>& out_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void process_once();

private:
    core::ThreadSafeQueue<core::VideoFrame>& in_queue_;
    core::ThreadSafeQueue<core::Event>& out_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::processing
