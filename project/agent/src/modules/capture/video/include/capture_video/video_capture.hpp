#pragma once

#include <atomic>
#include <string>

#include "foundation/module.hpp"
#include "foundation/thread_safe_queue.hpp"
#include "foundation/types.hpp"

namespace piguard::capture {

class VideoCapture : public foundation::Module {
public:
    explicit VideoCapture(foundation::ThreadSafeQueue<foundation::VideoFrame>& out_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void poll_once();

private:
    foundation::ThreadSafeQueue<foundation::VideoFrame>& out_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::capture
