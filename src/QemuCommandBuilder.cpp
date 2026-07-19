#include "QemuCommandBuilder.hpp"

#include <filesystem>
#include <sstream>

namespace wvm {

    std::string build_qemu_command(
        const std::filesystem::path& dist,
        const VMConfig& config
    ) {
        std::stringstream cmd;

        cmd << "qemu-system-" << config.arch << " ";
        cmd << "-machine " << config.machine << " ";

        const bool has_kvm = std::filesystem::exists("/dev/kvm");

        if (has_kvm) {
            cmd << "-enable-kvm ";
            cmd << "-cpu " << config.cpu << " ";
        } else {
            cmd << "-cpu max ";
        }

        cmd << "-m " << config.memory << " ";
        cmd << "-smp " << config.cores << " ";

        const auto disk_path = dist / config.disk_path;

        // VM의 기본 설치/시스템 디스크는 한 번만 추가한다.
        cmd << "-drive file=\""
            << disk_path.string()
            << "\",format=" << config.disk_format << " ";

        if (config.boot_mode == "disk") {
            cmd << "-boot c ";
        } else if (config.boot_mode == "iso") {
            const auto iso_path = std::filesystem::path(config.boot_path).is_absolute()
                ? std::filesystem::path(config.boot_path)
                : dist / config.boot_path;

            cmd << "-cdrom \""
                << iso_path.string()
                << "\" ";

            cmd << "-boot d ";
        } else if (config.boot_mode == "img") {
            const auto img_path = std::filesystem::path(config.boot_path).is_absolute()
                ? std::filesystem::path(config.boot_path)
                : dist / config.boot_path;

            cmd << "-drive file=\""
                << img_path.string()
                << "\",format=raw ";

            cmd << "-boot c ";
        }

        cmd << "-netdev user,id=net0 ";
        cmd << "-device virtio-net-pci,netdev=net0 ";
        cmd << "-display " << config.display;

        return cmd.str();
    }

}
