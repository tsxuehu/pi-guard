#include <iostream>

#include "piguard/version.hpp"

int main() {
    std::cout << "pi-guard-agent started, version " << piguard::kVersion << '\n';
    return 0;
}
