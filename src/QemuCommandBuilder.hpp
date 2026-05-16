#pragma once

#include "VMConfig.hpp"

#include <filesystem>
#include <string>

namespace wvm {
    std::string build_qemu_command(const std::filesystem::path& dist, const VMConfig& config);
}