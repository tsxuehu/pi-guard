#pragma once

#include <atomic>
#include <string>

#include "core/module.hpp"
#include "core/thread_safe_queue.hpp"
#include "core/types.hpp"

namespace piguard::output {

class FileWriter : public core::Module {
public:
    explicit FileWriter(core::ThreadSafeQueue<core::Event>& event_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void flush_once();

private:
    core::ThreadSafeQueue<core::Event>& event_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::output
