#include "Commands.hpp"
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    if (argc < 2 || std::string(argv[1]) != "host-setup") {
        std::cerr << "This helper only prepares local KVM access.\n";
        return 2;
    }
    try {
        return wvm::command_host_setup(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
