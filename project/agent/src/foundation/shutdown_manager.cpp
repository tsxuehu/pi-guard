#include "foundation/shutdown_manager.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>

namespace piguard::foundation {

namespace {
std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;
std::atomic<bool> g_ready_to_stop{false};
}  // namespace

void ShutdownManager::handle_signal(int signum) {
    if (signum != SIGINT && signum != SIGTERM) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_shutdown_mutex);
        g_ready_to_stop.store(true, std::memory_order_release);
    }
    g_shutdown_cv.notify_all();
}

void ShutdownManager::wait_for_shutdown() {
    std::unique_lock<std::mutex> lock(g_shutdown_mutex);
    g_shutdown_cv.wait(lock, [] {
        return g_ready_to_stop.load(std::memory_order_acquire);
    });
}

}  // namespace piguard::foundation
