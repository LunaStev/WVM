#include "ProcessRunner.hpp"
#include "QemuCommandBuilder.hpp"
#include "VMConfig.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::size_t find_argument(
    const wvm::Command& command,
    const std::string& argument
) {
    const auto iterator = std::find(command.begin(), command.end(), argument);
    return iterator == command.end()
        ? command.size()
        : static_cast<std::size_t>(iterator - command.begin());
}

std::size_t find_argument_containing(
    const wvm::Command& command,
    const std::string& fragment
) {
    const auto iterator = std::find_if(
        command.begin(),
        command.end(),
        [&](const std::string& argument) {
            return argument.find(fragment) != std::string::npos;
        }
    );
    return iterator == command.end()
        ? command.size()
        : static_cast<std::size_t>(iterator - command.begin());
}

void test_config_validation() {
    wvm::VMConfig config;
    std::string error;
    expect(wvm::validate_config(config, error), "default config is valid");

    config.arch = "x86_64;touch-pwned";
    expect(!wvm::validate_config(config, error), "unsafe architecture is rejected");

    config = wvm::VMConfig{};
    config.cores = 0;
    expect(!wvm::validate_config(config, error), "zero cores are rejected");

    config = wvm::VMConfig{};
    config.network_mode = "bridge";
    expect(!wvm::validate_config(config, error), "unsupported network mode is rejected");

    config = wvm::VMConfig{};
    config.acceleration = "unsafe";
    expect(!wvm::validate_config(config, error), "unsupported acceleration is rejected");
}

void test_iso_command() {
    wvm::VMConfig config;
    config.boot_mode = "iso";
    config.boot_path = "installer image.iso";

    const auto command = wvm::build_qemu_command(
        "/vm root",
        config,
        false,
        "/vm root/.wvm/qmp.sock"
    );
    expect(command.front() == "qemu-system-x86_64", "QEMU binary is selected by arch");
    expect(find_argument(command, "-enable-kvm") == command.size(), "KVM is optional");
    expect(find_argument(command, "tcg,thread=multi") < command.size(), "TCG uses MTTCG");
    expect(
        find_argument(command, "/vm root/installer image.iso") < command.size(),
        "ISO path remains one process argument"
    );
    expect(
        find_argument(command, "order=c,once=d") < command.size(),
        "ISO boots once before returning to disk"
    );
    expect(find_argument(command, "-netdev") < command.size(), "user network is enabled");
    expect(find_argument(command, "-qmp") < command.size(), "QMP control is enabled");
    expect(find_argument(command, "virtio") < command.size(), "VirtIO VGA is the default");
    expect(
        find_argument_containing(command, ",if=virtio") < command.size(),
        "VirtIO block is the default"
    );
}

void test_img_boot_order_and_drive_escaping() {
    wvm::VMConfig config;
    config.disk_path = "system,disk.qcow2";
    config.boot_mode = "img";
    config.boot_path = "boot.img";
    config.network_mode = "none";

    const auto command = wvm::build_qemu_command("/vm", config, false);
    const auto boot_image = find_argument_containing(command, "file=/vm/boot.img");
    const auto system_disk = find_argument_containing(command, "system,,disk.qcow2");

    expect(boot_image < system_disk, "raw IMG is enumerated before the system disk");
    expect(system_disk < command.size(), "commas in drive paths are escaped");
    expect(find_argument(command, "-netdev") == command.size(), "network none is respected");
    expect(find_argument(command, "-nic") < command.size(), "default networking is disabled");
}

void test_logging_and_process_status() {
    const wvm::Command command = {"program", "argument with spaces", "$(unsafe)"};
    const std::string formatted = wvm::format_command(command);
    expect(
        formatted == "program 'argument with spaces' '$(unsafe)'",
        "display formatting quotes shell metacharacters"
    );

    expect(wvm::run_process({"/bin/true"}) == 0, "successful exit status is normalized");
    expect(wvm::run_process({"/bin/false"}) == 1, "failed exit status is normalized");
    expect(
        wvm::run_process({"wvm-command-that-does-not-exist"}) == 127,
        "missing executable returns 127"
    );
}

void test_legacy_config_compatibility() {
    wvm::VMConfig config;
    expect(
        wvm::load_config("tests/LegacyConfig.xml", config),
        "legacy config fixture loads"
    );
    expect(config.disk_interface == "virtio", "legacy disks upgrade to VirtIO");
    expect(config.graphics == "virtio", "legacy graphics upgrade to VirtIO");
    expect(config.acceleration == "kvm", "legacy acceleration upgrades to KVM");
}

}

int main() {
    test_config_validation();
    test_iso_command();
    test_img_boot_order_and_drive_escaping();
    test_logging_and_process_status();
    test_legacy_config_compatibility();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All WVM tests passed.\n";
    return 0;
}
