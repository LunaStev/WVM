#include "Commands.hpp"
#include "ProcessRunner.hpp"
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
    fs::path dist = ".";
    if (argc >= 3 && !std::string(argv[2]).starts_with("--")) {
        dist = argv[2];
    }

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

    const Command command = build_qemu_command(
        dist,
        config,
        kvm_available(config.arch)
    );

    std::cout << format_command(command) << std::endl;
    if (dry_run) {
        return 0;
    }

    return run_process(command);
}

}
