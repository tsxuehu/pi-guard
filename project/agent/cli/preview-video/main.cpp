#include "foundation/shutdown_manager.hpp"
#include "frame_viewer_consumer.hpp"
#include "infra_log/logger.hpp"
#include "infra_log/logger_factory.hpp"

#include <opencv2/highgui.hpp>

#include <csignal>
#include <memory>
#include <string>
#include <thread>

namespace {
const std::shared_ptr<piguard::infra_log::Logger> logger = 
    piguard::infra_log::LogFactory::getLogger("PreviewVideoCli");
constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr int kCaptureFps = 30;
constexpr size_t kProviderQueueCapacity = 30;
constexpr char kWindowName[] = "pi-guard-view-video";
}  // namespace

int main(int argc, char** argv) {
    const std::string device = (argc > 1) ? argv[1] : "/dev/video0";

    if (std::signal(SIGINT, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR ||
        std::signal(SIGTERM, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR) {
        logger->error("failed to register signal handler");
        return 1;
    }

    auto provider = std::make_shared<piguard::capture_video::VideoCaptureProvider>(
        device, kCaptureFps, kWidth, kHeight, kProviderQueueCapacity);

    // 先注册消费者再启动采集，避免早期帧 pending_consumers 为空、无主驻留在队列里。
    FrameViewerConsumer consumer(provider, "viewer", kWidth, kHeight);
    provider->start();

    // GUI 操作（namedWindow / imshow / waitKey / destroyWindow）全部在同一线程完成，
    // 满足 OpenCV HighGUI 的线程一致性要求。主线程仅负责等关机信号。
    std::thread t_preview([&]() {
        cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);
        consumer.run();
        cv::destroyWindow(kWindowName);
        // 'q'/ESC 退出时主线程仍阻塞在 wait_for_shutdown，自发一次信号让其汇合。
        std::raise(SIGTERM);
    });

    logger->info("preview started, device=" + device + ", press Ctrl+C or 'q' to stop");

    piguard::foundation::ShutdownManager::wait_for_shutdown();
    logger->info("signal received, stopping provider");
    provider->stop();  // 唤醒 wait_frame，让 t_preview 退出

    t_preview.join();
    return 0;
}
