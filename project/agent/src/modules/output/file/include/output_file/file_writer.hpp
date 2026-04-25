#pragma once

#include <atomic>
#include <string>

#include "foundation/module.hpp"
#include "foundation/thread_safe_queue.hpp"
#include "foundation/types.hpp"

namespace piguard::output {

class FileWriter : public foundation::Module {
public:
    explicit FileWriter(foundation::ThreadSafeQueue<foundation::Event>& event_queue);

    std::string name() const override;
    bool start() override;
    void stop() override;
    void flush_once();

private:
    foundation::ThreadSafeQueue<foundation::Event>& event_queue_;
    std::atomic<bool> running_{false};
};

}  // namespace piguard::output
