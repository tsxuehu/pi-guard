#include <iostream>

#include "app/agent_app.hpp"
#include "piguard/version.hpp"

int main() {
    std::cout << "pi-guard-agent started, version " << piguard::kVersion << '\n';

    piguard::app::AgentApp app("config/agent.json");
    if (!app.start()) {
        std::cerr << "failed to start agent app\n";
        return 1;
    }

    app.run_for_demo();
    app.stop();
    return 0;
}
