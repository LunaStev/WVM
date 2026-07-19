#include "HostCapabilities.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace wvm {

namespace {

std::string compiled_host_architecture() {
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__aarch64__)
    return "aarch64";
#else
    return "unknown";
#endif
}

bool cpu_virtualization_flag_present() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    const std::string contents{
        std::istreambuf_iterator<char>(cpuinfo),
        std::istreambuf_iterator<char>()
    };

    return contents.find(" vmx ") != std::string::npos
        || contents.find(" svm ") != std::string::npos
        || (contents.find("Features") != std::string::npos
            && contents.find(" virt ") != std::string::npos);
}

}

KvmStatus inspect_kvm(const std::string& guest_architecture) {
    KvmStatus status;
    status.host_architecture = compiled_host_architecture();
    status.native_architecture = guest_architecture == status.host_architecture;
    status.cpu_virtualization = cpu_virtualization_flag_present();
    status.device_exists = std::filesystem::exists("/dev/kvm");
    status.device_accessible = access("/dev/kvm", R_OK | W_OK) == 0;
    return status;
}

bool gpu_acceleration_available() {
    const std::filesystem::path dri("/dev/dri");
    std::error_code error;
    if (!std::filesystem::is_directory(dri, error)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dri, error)) {
        if (error) {
            return false;
        }

        const std::string name = entry.path().filename().string();
        if (name.starts_with("renderD")
            && access(entry.path().c_str(), R_OK | W_OK) == 0) {
            return true;
        }
    }

    return false;
}

}
