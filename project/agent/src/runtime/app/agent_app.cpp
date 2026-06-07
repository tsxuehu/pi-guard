#include "agent_app.hpp"

#include <chrono>

namespace piguard::app {

AgentApp::AgentApp(std::string config_path) {}

AgentApp::~AgentApp() { stop(); }

bool AgentApp::start() {
    
    return true;
}

void AgentApp::stop() {
    
}

void AgentApp::run_for_demo() {
    
}

}  // namespace piguard::app
