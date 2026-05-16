#pragma once

#include <filesystem>
#include <string>

namespace wvm {

struct VMConfig {
    std::string name;
    std::string arch;
    std::string machine;
    std::string cpu;
    std::string memory;
    int cores;

    std::string disk_path;
    std::string disk_size;
    std::string disk_format;

    std::string boot_mode;
    std::string boot_path;

    std::string display;
    std::string network_mode;
};

bool save_config(const std::filesystem::path& path, const VMConfig& config);
bool load_config(const std::filesystem::path& path, VMConfig& config);

}