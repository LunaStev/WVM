#pragma once

#include <filesystem>
#include <string>

namespace wvm {

struct VMConfig {
    std::string name = "vm";
    std::string arch = "x86_64";
    std::string machine = "q35";
    std::string cpu = "host";
    std::string memory = "4G";
    int cores = 4;

    std::string disk_path = "disk.qcow2";
    std::string disk_size = "64G";
    std::string disk_format = "qcow2";

    std::string boot_mode = "none";
    std::string boot_path;

    std::string display = "gtk";
    std::string network_mode = "user";
};

bool save_config(const std::filesystem::path& path, const VMConfig& config);
bool load_config(const std::filesystem::path& path, VMConfig& config);
bool validate_config(const VMConfig& config, std::string& error);

}
