#include "Commands.hpp"
#include "HostCapabilities.hpp"
#include "ProcessRunner.hpp"
#include "QmpClient.hpp"
#include "QemuCommandBuilder.hpp"
#include "VMConfig.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace wvm {

namespace {

std::optional<std::string> get_arg_value(
    const int argc,
    char** argv,
    const std::string& key
) {
    for (int index = 2; index < argc; ++index) {
        if (argv[index] != key) {
            continue;
        }

        if (index + 1 >= argc) {
            return std::nullopt;
        }

        return std::string(argv[index + 1]);
    }

    return std::nullopt;
}

bool has_arg(const int argc, char** argv, const std::string& key) {
    for (int index = 2; index < argc; ++index) {
        if (argv[index] == key) {
            return true;
        }
    }
    return false;
}

bool option_is_missing_value(
    const int argc,
    char** argv,
    const std::string& key
) {
    for (int index = 2; index < argc; ++index) {
        if (argv[index] == key) {
            return index + 1 >= argc || std::string(argv[index + 1]).starts_with("--");
        }
    }
    return false;
}

bool parse_cores(const std::string& value, int& cores) {
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, cores);
    return result.ec == std::errc() && result.ptr == end;
}

fs::path resolve_path(const fs::path& dist, const std::string& path) {
    const fs::path candidate(path);
    return candidate.is_absolute() ? candidate : dist / candidate;
}

bool validate_and_report(const VMConfig& config) {
    std::string error;
    if (validate_config(config, error)) {
        return true;
    }

    std::cerr << "Invalid wvm.xml: " << error << "\n";
    return false;
}

bool boot_source_exists(const fs::path& dist, const VMConfig& config) {
    if (config.boot_mode != "iso" && config.boot_mode != "img") {
        return true;
    }

    const auto boot_path = resolve_path(dist, config.boot_path);
    if (fs::is_regular_file(boot_path)) {
        return true;
    }

    std::cerr << "Boot source does not exist or is not a regular file: "
              << boot_path << "\n";
    return false;
}

fs::path command_dist(const int argc, char** argv) {
    if (argc >= 3 && !std::string(argv[2]).starts_with("--")) {
        return argv[2];
    }
    return ".";
}

fs::path qmp_socket_path(const fs::path& dist) {
    return fs::absolute(dist) / ".wvm" / "qmp.sock";
}

bool execute_qmp_command(
    const fs::path& dist,
    const std::string& command,
    std::string& response
) {
    const fs::path socket_path = qmp_socket_path(dist);
    if (!fs::exists(socket_path)) {
        std::cerr << "VM is not running (QMP socket not found).\n";
        return false;
    }

    QmpClient client;
    std::string error;
    if (!client.connect(socket_path, error)
        || !client.execute(command, response, error)) {
        std::cerr << error << "\n";
        return false;
    }

    return true;
}

std::string qmp_status_value(const std::string& response) {
    const std::string key = "\"status\"";
    const std::size_t key_position = response.find(key);
    if (key_position == std::string::npos) {
        return "unknown";
    }

    const std::size_t colon = response.find(':', key_position + key.size());
    if (colon == std::string::npos) {
        return "unknown";
    }
    const std::size_t opening_quote = response.find('"', colon + 1);
    if (opening_quote == std::string::npos) {
        return "unknown";
    }
    const std::size_t closing_quote = response.find('"', opening_quote + 1);
    if (closing_quote == std::string::npos) {
        return "unknown";
    }

    return response.substr(opening_quote + 1, closing_quote - opening_quote - 1);
}

}

int command_init(int, char**) {
    const fs::path dir = fs::current_path();
    const fs::path config_path = dir / "wvm.xml";

    if (fs::exists(config_path)) {
        std::cerr << "wvm.xml already exists.\n";
        return 1;
    }

    VMConfig config;
    config.name = dir.filename().string();

    if (!validate_and_report(config) || !save_config(config_path, config)) {
        std::cerr << "Failed to create wvm.xml.\n";
        return 1;
    }

    std::cout << "Initialized WVM project: " << config_path << "\n";
    return 0;
}

int command_set(const int argc, char** argv) {
    if (option_is_missing_value(argc, argv, "--dist")
        || option_is_missing_value(argc, argv, "--size")
        || option_is_missing_value(argc, argv, "--memory")
        || option_is_missing_value(argc, argv, "--cores")) {
        std::cerr << "A set option is missing its value.\n";
        return 2;
    }

    const std::string dist_value = get_arg_value(argc, argv, "--dist").value_or(".");
    const fs::path dist(dist_value);
    const fs::path config_path = dist / "wvm.xml";

    VMConfig config;
    if (!load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    const std::string previous_size = config.disk_size;
    const auto size = get_arg_value(argc, argv, "--size");
    const auto memory = get_arg_value(argc, argv, "--memory");
    const auto cores = get_arg_value(argc, argv, "--cores");

    if (size) {
        config.disk_size = *size;
    }
    if (memory) {
        config.memory = *memory;
    }
    if (cores && !parse_cores(*cores, config.cores)) {
        std::cerr << "Invalid core count: " << *cores << "\n";
        return 2;
    }

    if (!validate_and_report(config)) {
        return 2;
    }

    const fs::path disk_path = resolve_path(dist, config.disk_path);
    Command disk_command;

    if (!fs::exists(disk_path)) {
        disk_command = {
            "qemu-img", "create", "-f", config.disk_format,
            disk_path.string(), config.disk_size
        };
    } else if (size && config.disk_size != previous_size) {
        disk_command = {
            "qemu-img", "resize", "-f", config.disk_format,
            disk_path.string(), config.disk_size
        };
    }

    if (!disk_command.empty()) {
        std::cout << format_command(disk_command) << std::endl;
        const int result = run_process(disk_command);
        if (result != 0) {
            return result;
        }
    }

    if (!save_config(config_path, config)) {
        std::cerr << "Failed to save wvm.xml.\n";
        return 1;
    }

    std::cout << "Updated WVM config.\n";
    return 0;
}

int command_install(const int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: wvm install . (--iso os.iso | --img os.img)\n";
        return 2;
    }

    if (option_is_missing_value(argc, argv, "--iso")
        || option_is_missing_value(argc, argv, "--img")) {
        std::cerr << "An install option is missing its path.\n";
        return 2;
    }

    const fs::path dist = argv[2];
    const fs::path config_path = dist / "wvm.xml";

    VMConfig config;
    if (!load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    const auto iso = get_arg_value(argc, argv, "--iso");
    const auto img = get_arg_value(argc, argv, "--img");
    if (iso.has_value() == img.has_value()) {
        std::cerr << "Specify exactly one of --iso or --img.\n";
        return 2;
    }

    config.boot_mode = iso ? "iso" : "img";
    config.boot_path = iso ? *iso : *img;

    if (!validate_and_report(config) || !boot_source_exists(dist, config)) {
        return 2;
    }

    if (!save_config(config_path, config)) {
        std::cerr << "Failed to save wvm.xml.\n";
        return 1;
    }

    std::cout << "Install source configured.\n";
    return 0;
}

int command_run(const int argc, char** argv) {
    const fs::path dist = command_dist(argc, argv);

    if (option_is_missing_value(argc, argv, "--iso")
        || option_is_missing_value(argc, argv, "--img")) {
        std::cerr << "A run option is missing its path.\n";
        return 2;
    }

    const fs::path config_path = dist / "wvm.xml";
    VMConfig config;
    if (!load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    const auto iso = get_arg_value(argc, argv, "--iso");
    const auto img = get_arg_value(argc, argv, "--img");
    const bool disk = has_arg(argc, argv, "--disk");
    const int boot_overrides = static_cast<int>(iso.has_value())
        + static_cast<int>(img.has_value()) + static_cast<int>(disk);

    if (boot_overrides > 1) {
        std::cerr << "Specify only one of --disk, --iso or --img.\n";
        return 2;
    }

    if (disk) {
        config.boot_mode = "disk";
        config.boot_path.clear();
    } else if (iso) {
        config.boot_mode = "iso";
        config.boot_path = *iso;
    } else if (img) {
        config.boot_mode = "img";
        config.boot_path = *img;
    }

    if (!validate_and_report(config) || !boot_source_exists(dist, config)) {
        return 2;
    }

    const fs::path disk_path = resolve_path(dist, config.disk_path);
    if (!fs::is_regular_file(disk_path)) {
        std::cerr << "VM disk does not exist or is not a regular file: "
                  << disk_path << "\n";
        return 1;
    }

    const bool dry_run = has_arg(argc, argv, "--dry-run");
    if (boot_overrides != 0 && !dry_run && !save_config(config_path, config)) {
        std::cerr << "Failed to save wvm.xml.\n";
        return 1;
    }

    const fs::path socket_path = qmp_socket_path(dist);
    std::string socket_error;
    if (!validate_qmp_socket_path(socket_path, socket_error)) {
        std::cerr << socket_error << "\n";
        return 1;
    }
    const KvmStatus kvm = inspect_kvm(config.arch);
    if (!kvm.native_architecture) {
        std::cerr << "WVM currently requires a native guest architecture for hardware acceleration.\n";
        return 1;
    }
    if (!kvm.available()) {
        std::cerr << "KVM hardware acceleration is required. Run 'wvm doctor "
                  << dist.string() << "' for details.\n";
        return 1;
    }

    const Command command = build_qemu_command(
        dist,
        config,
        true,
        socket_path,
        gpu_acceleration_available()
    );

    std::cout << format_command(command) << std::endl;
    if (dry_run) {
        return 0;
    }

    fs::create_directories(socket_path.parent_path());
    if (fs::exists(socket_path)) {
        QmpClient existing_client;
        std::string error;
        if (existing_client.connect(socket_path, error)) {
            std::cerr << "VM is already running: " << socket_path << "\n";
            return 1;
        }

        if (!has_arg(argc, argv, "--recover")) {
            std::cerr << "QMP socket exists but is unreachable: " << socket_path
                      << "\nUse --recover only after confirming no QEMU process is running.\n";
            return 1;
        }

        std::error_code remove_error;
        fs::remove(socket_path, remove_error);
        if (remove_error) {
            std::cerr << "Failed to remove stale QMP socket: "
                      << remove_error.message() << "\n";
            return 1;
        }
    }

    const int result = run_process(command);

    std::error_code cleanup_error;
    fs::remove(socket_path, cleanup_error);

    if (result == 0 && config.boot_mode == "iso") {
        config.boot_mode = "disk";
        config.boot_path.clear();
        if (!save_config(config_path, config)) {
            std::cerr << "VM stopped, but failed to switch future boots to disk.\n";
            return 1;
        }
        std::cout << "Installation media consumed; future boots will use the VM disk.\n";
    }

    return result;
}

int command_status(const int argc, char** argv) {
    const fs::path dist = command_dist(argc, argv);
    const fs::path socket_path = qmp_socket_path(dist);
    if (!fs::exists(socket_path)) {
        std::cout << "stopped\n";
        return 0;
    }

    std::string response;
    if (!execute_qmp_command(dist, "query-status", response)) {
        return 1;
    }

    std::cout << qmp_status_value(response) << "\n";
    return 0;
}

int command_stop(const int argc, char** argv) {
    const fs::path dist = command_dist(argc, argv);
    const bool force = has_arg(argc, argv, "--force");
    std::string response;
    if (!execute_qmp_command(dist, force ? "quit" : "system_powerdown", response)) {
        return 1;
    }

    std::cout << (force ? "Forced stop requested.\n" : "Graceful shutdown requested.\n");
    return 0;
}

int command_reboot(const int argc, char** argv) {
    const fs::path dist = command_dist(argc, argv);
    std::string response;
    if (!execute_qmp_command(dist, "system_reset", response)) {
        return 1;
    }

    std::cout << "Reset requested.\n";
    return 0;
}

int command_doctor(const int argc, char** argv) {
    const fs::path dist = command_dist(argc, argv);
    VMConfig config;
    const fs::path config_path = dist / "wvm.xml";
    if (fs::exists(config_path) && !load_config(config_path, config)) {
        std::cerr << "Failed to load wvm.xml.\n";
        return 1;
    }

    const KvmStatus status = inspect_kvm(config.arch);
    const bool gpu_acceleration = gpu_acceleration_available();
    const std::string effective_acceleration = status.available()
        ? "KVM" : "unavailable (KVM required)";

    std::cout << "Host architecture: " << status.host_architecture << "\n"
              << "Guest architecture: " << config.arch << "\n"
              << "Configured acceleration: " << config.acceleration << "\n"
              << "Native architecture: " << (status.native_architecture ? "yes" : "no") << "\n"
              << "CPU virtualization flag: " << (status.cpu_virtualization ? "yes" : "no") << "\n"
              << "/dev/kvm present: " << (status.device_exists ? "yes" : "no") << "\n"
              << "/dev/kvm accessible: " << (status.device_accessible ? "yes" : "no") << "\n"
              << "Effective acceleration: " << effective_acceleration << "\n"
              << "GPU render node: " << (gpu_acceleration ? "yes" : "no") << "\n"
              << "Graphics path: " << (gpu_acceleration ? "VirtIO GL" : "VirtIO 2D") << "\n";

    if (!status.native_architecture) {
        std::cout << "Cross-architecture guests require software emulation.\n";
        return 0;
    }
    if (!status.cpu_virtualization && !status.available()) {
        std::cout << "Enable AMD SVM or Intel VT-x in the system firmware.\n";
    }
    if (!status.device_exists) {
        std::cout << "Load the kvm and vendor KVM kernel modules, then verify /dev/kvm.\n";
    } else if (!status.device_accessible) {
        std::cout << "Grant this user access to /dev/kvm (normally through the kvm group).\n";
    }
    return status.available() ? 0 : 1;
}

}
