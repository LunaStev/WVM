#include "QemuCommandBuilder.hpp"

#include <sstream>
#include <filesystem>

namespace wvm {

    std::string build_qemu_command(const std::filesystem::path& dist, const VMConfig& config) {
        std::stringstream cmd;

        cmd << "qemu-system-" << config.arch << " ";
        cmd << "-machine " << config.machine << " ";

        bool has_kvm = std::filesystem::exists("/dev/kvm");

        if (has_kvm) {
            cmd << "-enable-kvm ";
            cmd << "-cpu " << config.cpu << " ";
        } else {
            cmd << "-cpu max ";
        }

        cmd << "-m " << config.memory << " ";
        cmd << "-smp " << config.cores << " ";

        auto disk_path = dist / config.disk_path;

        cmd << "-drive file=\"" << disk_path.string() << "\",format=" << config.disk_format << " ";

        if (config.boot_mode == "disk") {
            auto boot_disk_path = dist / config.boot_path;

            cmd << "-drive file=\""
                << boot_disk_path.string()
                << "\",format=" << config.disk_format << " ";
        } else if (config.boot_mode == "iso") {
            auto disk_path = dist / config.disk_path;
            auto iso_path = dist / config.boot_path;

            cmd << "-drive file=\""
                << disk_path.string()
                << "\",format=" << config.disk_format << " ";

            cmd << "-cdrom \""
                << iso_path.string()
                << "\" ";

            cmd << "-boot d ";
        } else if (config.boot_mode == "img") {
            auto img_path = dist / config.boot_path;

            cmd << "-drive file=\""
                << img_path.string()
                << "\",format=raw ";
        } else {
            auto disk_path = dist / config.disk_path;

            cmd << "-drive file=\""
                << disk_path.string()
                << "\",format=" << config.disk_format << " ";
        }

        cmd << "-netdev user,id=net0 ";
        cmd << "-device virtio-net-pci,netdev=net0 ";
        cmd << "-display " << config.display;

        return cmd.str();
    }

}