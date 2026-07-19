#pragma once

#include "VMConfig.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wvm {
    using Command = std::vector<std::string>;

    bool kvm_available(const std::string& guest_architecture);
    Command build_qemu_command(
        const std::filesystem::path& dist,
        const VMConfig& config,
        bool enable_kvm,
        const std::optional<std::filesystem::path>& qmp_socket = std::nullopt
    );
    std::string format_command(const Command& command);
}
