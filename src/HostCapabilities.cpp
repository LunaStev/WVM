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

std::string read_cpuinfo() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    return {
        std::istreambuf_iterator<char>(cpuinfo),
        std::istreambuf_iterator<char>()
    };
}

bool cpu_virtualization_flag_present(const std::string& contents) {
    return contents.find(" vmx ") != std::string::npos
        || contents.find(" svm ") != std::string::npos
        || (contents.find("Features") != std::string::npos
            && contents.find(" virt ") != std::string::npos);
}

std::string cpu_vendor(const std::string& contents) {
    if (contents.find("AuthenticAMD") != std::string::npos) {
        return "AMD";
    }
    if (contents.find("GenuineIntel") != std::string::npos) {
        return "Intel";
    }
    return "unknown";
}

}

KvmStatus inspect_kvm(const std::string& guest_architecture) {
    const std::string cpuinfo = read_cpuinfo();
    KvmStatus status;
    status.host_architecture = compiled_host_architecture();
    status.cpu_vendor = cpu_vendor(cpuinfo);
    status.native_architecture = guest_architecture == status.host_architecture;
    status.cpu_virtualization = cpu_virtualization_flag_present(cpuinfo);
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
