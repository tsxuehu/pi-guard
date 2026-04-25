#include <cassert>
#include <chrono>
#include <future>
#include <thread>

#include "foundation/thread_safe_queue.hpp"
#include "foundation/types.hpp"

using piguard::foundation::Event;
using piguard::foundation::EventType;
using piguard::foundation::ThreadSafeQueue;

int main() {
    ThreadSafeQueue<int> queue;
    assert(!queue.pop().has_value());
    assert(queue.size() == 0U);

    queue.push(7);
    queue.push(9);
    assert(queue.size() == 2U);

    const auto first = queue.pop();
    assert(first.has_value() && *first == 7);
    const auto second = queue.wait_and_pop();
    assert(second == 9);
    assert(queue.size() == 0U);

    ThreadSafeQueue<Event> event_queue;
    auto waiter = std::async(std::launch::async, [&event_queue]() { return event_queue.wait_and_pop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    event_queue.push(Event{EventType::MotionStart, 42, "payload"});

    const Event event = waiter.get();
    assert(event.type == EventType::MotionStart);
    assert(event.timestamp_ms == 42);
    assert(event.payload == "payload");
    return 0;
}
