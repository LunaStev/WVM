#include "Commands.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

int run(const int argc, char** argv) {
    if (argc < 2) {
        std::cout << "WVM - Wave Virtual Machine Manager\n";
        std::cout << "Usage:\n";
        std::cout << "  wvm init\n";
        std::cout << "  wvm set --dist . --size 64G --memory 4G --cores 4\n";
        std::cout << "  wvm install . --iso os.iso\n";
        std::cout << "  wvm install . --img os.img\n";
        std::cout << "  wvm run . [--disk | --iso os.iso | --img os.img] [--dry-run] [--recover]\n";
        std::cout << "  wvm status .\n";
        std::cout << "  wvm stop . [--force]\n";
        std::cout << "  wvm reboot .\n";
        std::cout << "  wvm doctor .\n";
        return 0;
    }

    std::string command = argv[1];

    if (command == "init") {
        return wvm::command_init(argc, argv);
    }

    if (command == "set") {
        return wvm::command_set(argc, argv);
    }

    if (command == "install") {
        return wvm::command_install(argc, argv);
    }

    if (command == "run") {
        return wvm::command_run(argc, argv);
    }

    if (command == "status") {
        return wvm::command_status(argc, argv);
    }

    if (command == "stop") {
        return wvm::command_stop(argc, argv);
    }

    if (command == "reboot") {
        return wvm::command_reboot(argc, argv);
    }

    if (command == "doctor") {
        return wvm::command_doctor(argc, argv);
    }

    std::cerr << "Unknown command: " << command << std::endl;
    return 1;
}

}

int main(const int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "WVM failed: " << error.what() << '\n';
        return 1;
    }
}
