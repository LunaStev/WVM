#include "QemuCommandBuilder.hpp"

#include <filesystem>
#include <sstream>
#include <string_view>

namespace wvm {

namespace {

std::filesystem::path resolve_path(
    const std::filesystem::path& dist,
    const std::string& path
) {
    const std::filesystem::path candidate(path);
    return candidate.is_absolute() ? candidate : dist / candidate;
}

std::string escape_drive_value(const std::filesystem::path& path) {
    std::string escaped;
    const std::string value = path.string();
    escaped.reserve(value.size());

    for (const char character : value) {
        escaped.push_back(character);
        if (character == ',') {
            escaped.push_back(',');
        }
    }

    return escaped;
}

std::string shell_quote(std::string_view argument) {
    if (!argument.empty() && argument.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./,:+=-"
        ) == std::string_view::npos) {
        return std::string(argument);
    }

    std::string quoted = "'";
    for (const char character : argument) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

void append_drive(
    Command& command,
    const std::filesystem::path& path,
    const std::string& format,
    const std::string& interface
) {
    command.emplace_back("-drive");
    command.emplace_back(
        "file=" + escape_drive_value(path) + ",format=" + format
            + ",if=" + interface
    );
}

}

Command build_qemu_command(
    const std::filesystem::path& dist,
    const VMConfig& config,
    const bool enable_kvm,
    const std::optional<std::filesystem::path>& qmp_socket,
    const bool enable_gpu_acceleration
) {
    Command command = {
        "qemu-system-" + config.arch,
        "-name", config.name,
        "-machine", config.machine,
        "-accel", enable_kvm ? "kvm" : "tcg,thread=multi",
        "-cpu", enable_kvm ? config.cpu : "max",
        "-m", config.memory,
        "-smp", "cpus=" + std::to_string(config.cores)
            + ",sockets=1,cores=" + std::to_string(config.cores) + ",threads=1"
    };

    const auto disk_path = resolve_path(dist, config.disk_path);

    // Raw boot images must be enumerated before the persistent system disk so
    // legacy BIOS boot order=c selects the requested image.
    if (config.boot_mode == "img") {
        append_drive(
            command,
            resolve_path(dist, config.boot_path),
            "raw",
            config.disk_interface
        );
        append_drive(command, disk_path, config.disk_format, config.disk_interface);
        command.insert(command.end(), {"-boot", "order=c"});
    } else {
        append_drive(command, disk_path, config.disk_format, config.disk_interface);

        if (config.boot_mode == "disk") {
            command.insert(command.end(), {"-boot", "order=c"});
        } else if (config.boot_mode == "iso") {
            command.insert(command.end(), {
                "-cdrom", resolve_path(dist, config.boot_path).string(),
                "-boot", "order=c,once=d"
            });
        }
    }

    if (config.network_mode == "user") {
        command.insert(command.end(), {
            "-netdev", "user,id=net0",
            "-device", "virtio-net-pci,netdev=net0"
        });
    } else {
        command.insert(command.end(), {"-nic", "none"});
    }

    if (qmp_socket) {
        command.insert(command.end(), {
            "-qmp",
            "unix:" + escape_drive_value(*qmp_socket) + ",server=on,wait=off"
        });
    }

    if (enable_gpu_acceleration
        && (config.display == "gtk" || config.display == "sdl")) {
        command.insert(command.end(), {"-device", "virtio-vga-gl"});
        command.insert(command.end(), {"-display", config.display + ",gl=on"});
    } else {
        command.insert(command.end(), {
            "-vga", config.display == "none" ? "none" : config.graphics
        });
        command.insert(command.end(), {"-display", config.display});
    }
    return command;
}

std::string format_command(const Command& command) {
    std::ostringstream output;

    for (std::size_t index = 0; index < command.size(); ++index) {
        if (index != 0) {
            output << ' ';
        }
        output << shell_quote(command[index]);
    }

    return output.str();
}

}
