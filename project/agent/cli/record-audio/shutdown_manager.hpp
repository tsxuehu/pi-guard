#pragma once

class ShutdownManager {
public:
    static void handle_signal(int signum);
    static void wait_for_shutdown();
};
