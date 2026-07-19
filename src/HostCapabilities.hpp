#pragma once

#include <string>

namespace wvm {

struct KvmStatus {
    std::string host_architecture;
    bool native_architecture = false;
    bool cpu_virtualization = false;
    bool device_exists = false;
    bool device_accessible = false;

    [[nodiscard]] bool available() const {
        return native_architecture && device_accessible;
    }
};

KvmStatus inspect_kvm(const std::string& guest_architecture);
bool gpu_acceleration_available();

}
