#pragma once

#include "VMConfig.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wvm {
    using Command = std::vector<std::string>;

    Command build_qemu_command(
        const std::filesystem::path& dist,
        const VMConfig& config,
        bool enable_kvm,
        const std::optional<std::filesystem::path>& qmp_socket = std::nullopt,
        bool enable_gpu_acceleration = false
    );
    std::string format_command(const Command& command);
}
