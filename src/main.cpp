#include "Commands.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "WVM - Wave Virtual Machine Manager\n";
        std::cout << "Usage:\n";
        std::cout << "  wvm init\n";
        std::cout << "  wvm set --dist . --size 64G --memory 4G --cores 4\n";
        std::cout << "  wvm install . --iso os.iso\n";
        std::cout << "  wvm install . --img os.img\n";
        std::cout << "  wvm run .\n";
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

    std::cerr << "Unknown command: " << command << std::endl;
    return 1;
}