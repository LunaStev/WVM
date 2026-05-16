#include "Commands.hpp"
#include "VMConfig.hpp"
#include "QemuCommandBuilder.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace wvm {

static std::string get_arg_value(int argc, char** argv, const std::string& key) {
    for (int i = 0; i < argc - 1; i++) {
        if (argv[i] == key) {
            return argv[i + 1];
        }
    }

    return "";
}

int command_init(int argc, char** argv) {
    fs::path dir = fs::current_path();
    fs::path config_path = dir / "wvm.xml";

    if (fs::exists(config_path)) {
        std::cerr << "wvm.xml already exists.\n";
        return 1;
    }

    VMConfig config;
    config.name = dir.filename().string();
    config.arch = "x86_64";
    config.machine = "q35";
    config.cpu = "host";
    config.memory = "4G";
    config.cores = 4;
    config.disk_path = "disk.qcow2";
    config.disk_size = "64G";
    config.disk_format = "qcow2";
    config.boot_mode = "none";
    config.boot_path = "";
    config.display = "gtk";
    config.network_mode = "user";

    if (!save_config(config_path, config)) {
        std::cerr << "Failed to create wvm.xml.\n";
        return 1;
    }

    std::cout << "Initialized WVM project: " << config_path << "\n";
    return 0;
}

int command_set(int argc, char** argv) {
    std::string dist = get_arg_value(argc, argv, "--dist");
    if (dist.empty()) {
        dist = ".";
    }

    fs::path config_path = fs::path(dist) / "wvm.xml";

    VMConfig config;
    if (!load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    std::string size = get_arg_value(argc, argv, "--size");
    std::string memory = get_arg_value(argc, argv, "--memory");
    std::string cores = get_arg_value(argc, argv, "--cores");

    if (!size.empty()) {
        config.disk_size = size;
    }

    if (!memory.empty()) {
        config.memory = memory;
    }

    if (!cores.empty()) {
        config.cores = std::stoi(cores);
    }

    if (!save_config(config_path, config)) {
        std::cerr << "Failed to save wvm.xml.\n";
        return 1;
    }

    fs::path disk_path = fs::path(dist) / config.disk_path;

    if (!fs::exists(disk_path)) {
        std::string cmd = "qemu-img create -f qcow2 \"" + disk_path.string() + "\" " + config.disk_size;
        std::cout << cmd << "\n";
        return std::system(cmd.c_str());
    }

    std::cout << "Updated WVM config.\n";
    return 0;
}

int command_install(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: wvm install . --iso os.iso\n";
        return 1;
    }

    fs::path dist = argv[2];
    fs::path config_path = dist / "wvm.xml";

    VMConfig config;
    if (!load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    std::string iso = get_arg_value(argc, argv, "--iso");
    std::string img = get_arg_value(argc, argv, "--img");

    if (!iso.empty()) {
        config.boot_mode = "iso";
        config.boot_path = iso;
    } else if (!img.empty()) {
        config.boot_mode = "img";
        config.disk_path = img;
        config.disk_format = "raw";
    } else {
        std::cerr << "Expected --iso or --img.\n";
        return 1;
    }

    if (!save_config(config_path, config)) {
        std::cerr << "Failed to save wvm.xml.\n";
        return 1;
    }

    std::cout << "Install source configured.\n";
    return 0;
}

int command_run(int argc, char** argv) {
    fs::path dist = ".";

    if (argc >= 3) {
        dist = argv[2];
    }

    fs::path config_path = dist / "wvm.xml";

    VMConfig config;
    if (!load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    std::string command = build_qemu_command(dist, config);

    std::cout << command << "\n";
    return std::system(command.c_str());
}

}