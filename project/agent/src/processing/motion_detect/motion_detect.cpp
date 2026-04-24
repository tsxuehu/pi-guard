#include "processing/motion_detect/motion_detect.hpp"

namespace piguard::processing {

MotionDetect::MotionDetect(core::ThreadSafeQueue<core::VideoFrame>& in_queue,
                           core::ThreadSafeQueue<core::Event>& out_queue)
    : in_queue_(in_queue), out_queue_(out_queue) {}

std::string MotionDetect::name() const { return "MotionDetect"; }

bool MotionDetect::start() {
    running_.store(true);
    return true;
}

void MotionDetect::stop() { running_.store(false); }

void MotionDetect::process_once() {
    if (!running_.load()) {
        return;
    }
    const auto frame = in_queue_.pop();
    if (!frame.has_value()) {
        return;
    }
    core::Event event{};
    event.type = core::EventType::MotionStart;
    event.timestamp_ms = frame->pts_ms;
    event.payload = "mock motion";
    out_queue_.push(std::move(event));
}

}  // namespace piguard::processing
