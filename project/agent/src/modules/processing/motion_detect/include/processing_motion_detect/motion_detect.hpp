#pragma once

#include <atomic>
#include <string>

#include "foundation/module.hpp"
#include "foundation/thread_safe_queue.hpp"
#include "foundation/types.hpp"

namespace piguard::processing {

class MotionDetect : public foundation::Module {
public:
    MotionDetect(foundation::ThreadSafeQueue<foundation::VideoFrame>& in_queue, foundation::ThreadSafeQueue<foundation::Event>& out_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void process_once();

private:
    foundation::ThreadSafeQueue<foundation::VideoFrame>& in_queue_;
    foundation::ThreadSafeQueue<foundation::Event>& out_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::processing
