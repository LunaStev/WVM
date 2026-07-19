#include "VMConfig.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <string>

namespace wvm {

static std::string text_or(const pugi::xml_node& node, const char* fallback) {
    if (!node) {
        return fallback;
    }

    const char* value = node.text().as_string();

    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }

    return value;
}

static int int_or(const pugi::xml_node& node, int fallback) {
    if (!node) {
        return fallback;
    }

    return node.text().as_int(fallback);
}

static bool contains_control_character(const std::string& value) {
    return std::any_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::iscntrl(character) != 0;
    });
}

static bool is_identifier(const std::string& value) {
    return !value.empty() && std::all_of(
        value.begin(),
        value.end(),
        [](const unsigned char character) {
            return std::isalnum(character) != 0 || character == '_' || character == '-';
        }
    );
}

static bool is_resource_size(const std::string& value) {
    if (value.empty() || value.front() == '0') {
        return false;
    }

    std::size_t digits = 0;
    while (digits < value.size() && std::isdigit(
        static_cast<unsigned char>(value[digits])
    ) != 0) {
        ++digits;
    }

    if (digits == 0) {
        return false;
    }

    if (digits == value.size()) {
        return true;
    }

    return digits + 1 == value.size()
        && std::string("KMGTPE").find(value.back()) != std::string::npos;
}

static bool is_one_of(
    const std::string& value,
    const std::initializer_list<const char*> choices
) {
    return std::any_of(choices.begin(), choices.end(), [&](const char* choice) {
        return value == choice;
    });
}

bool save_config(const std::filesystem::path& path, const VMConfig& config) {
    pugi::xml_document doc;

    auto declaration = doc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "UTF-8";

    auto root = doc.append_child("wvm");
    root.append_attribute("version") = "1";

    auto machine = root.append_child("machine");
    machine.append_child("name").text().set(config.name.c_str());
    machine.append_child("arch").text().set(config.arch.c_str());
    machine.append_child("type").text().set(config.machine.c_str());
    machine.append_child("cpu").text().set(config.cpu.c_str());
    machine.append_child("acceleration").text().set(config.acceleration.c_str());
    machine.append_child("memory").text().set(config.memory.c_str());
    machine.append_child("cores").text().set(config.cores);

    auto disk = root.append_child("disk");
    disk.append_child("path").text().set(config.disk_path.c_str());
    disk.append_child("size").text().set(config.disk_size.c_str());
    disk.append_child("format").text().set(config.disk_format.c_str());
    disk.append_child("interface").text().set(config.disk_interface.c_str());

    auto boot = root.append_child("boot");
    boot.append_child("mode").text().set(config.boot_mode.c_str());
    boot.append_child("path").text().set(config.boot_path.c_str());

    auto display = root.append_child("display");
    display.append_child("type").text().set(config.display.c_str());
    display.append_child("graphics").text().set(config.graphics.c_str());

    auto network = root.append_child("network");
    network.append_child("mode").text().set(config.network_mode.c_str());

    return doc.save_file(path.string().c_str(), "    ");
}

bool load_config(const std::filesystem::path& path, VMConfig& config) {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(path.string().c_str());

    if (!result) {
        return false;
    }

    auto root = doc.child("wvm");
    if (!root) {
        return false;
    }

    auto machine = root.child("machine");
    auto disk = root.child("disk");
    auto boot = root.child("boot");
    auto display = root.child("display");
    auto network = root.child("network");

    config.name = text_or(machine.child("name"), "vm");
    config.arch = text_or(machine.child("arch"), "x86_64");
    config.machine = text_or(machine.child("type"), "q35");
    config.cpu = text_or(machine.child("cpu"), "host");
    config.acceleration = text_or(machine.child("acceleration"), "kvm");
    config.memory = text_or(machine.child("memory"), "4G");
    config.cores = int_or(machine.child("cores"), 4);

    config.disk_path = text_or(disk.child("path"), "disk.qcow2");
    config.disk_size = text_or(disk.child("size"), "64G");
    config.disk_format = text_or(disk.child("format"), "qcow2");
    config.disk_interface = text_or(disk.child("interface"), "virtio");

    config.boot_mode = text_or(boot.child("mode"), "none");
    config.boot_path = text_or(boot.child("path"), "");

    config.display = text_or(display.child("type"), "gtk");
    config.graphics = text_or(display.child("graphics"), "virtio");
    config.network_mode = text_or(network.child("mode"), "user");

    return true;
}

bool validate_config(const VMConfig& config, std::string& error) {
    if (config.name.empty() || config.name.size() > 128
        || contains_control_character(config.name)) {
        error = "VM name must be 1-128 characters without control characters.";
        return false;
    }

    if (!is_identifier(config.arch)) {
        error = "Architecture may contain only letters, numbers, '_' and '-'.";
        return false;
    }

    if (config.machine.empty() || contains_control_character(config.machine)) {
        error = "Machine type must not be empty or contain control characters.";
        return false;
    }

    if (config.cpu.empty() || contains_control_character(config.cpu)) {
        error = "CPU model must not be empty or contain control characters.";
        return false;
    }

    if (!is_one_of(config.acceleration, {"auto", "kvm", "tcg"})) {
        error = "Acceleration must be one of: auto, kvm, tcg.";
        return false;
    }

    if (!is_resource_size(config.memory)) {
        error = "Memory must be a positive integer with an optional K/M/G/T/P/E suffix.";
        return false;
    }

    if (config.cores < 1 || config.cores > 1024) {
        error = "CPU core count must be between 1 and 1024.";
        return false;
    }

    if (config.disk_path.empty() || contains_control_character(config.disk_path)) {
        error = "Disk path must not be empty or contain control characters.";
        return false;
    }

    if (!is_resource_size(config.disk_size)) {
        error = "Disk size must be a positive integer with an optional K/M/G/T/P/E suffix.";
        return false;
    }

    if (!is_one_of(config.disk_format, {"qcow2", "raw"})) {
        error = "Disk format must be 'qcow2' or 'raw'.";
        return false;
    }

    if (!is_one_of(config.disk_interface, {"virtio", "ide"})) {
        error = "Disk interface must be 'virtio' or 'ide'.";
        return false;
    }

    if (!is_one_of(config.boot_mode, {"none", "disk", "iso", "img"})) {
        error = "Boot mode must be one of: none, disk, iso, img.";
        return false;
    }

    if ((config.boot_mode == "iso" || config.boot_mode == "img")
        && (config.boot_path.empty() || contains_control_character(config.boot_path))) {
        error = "ISO and IMG boot modes require a valid boot path.";
        return false;
    }

    if (!is_one_of(config.display, {"gtk", "sdl", "curses", "none"})) {
        error = "Display type must be one of: gtk, sdl, curses, none.";
        return false;
    }

    if (!is_one_of(config.graphics, {"virtio", "std"})) {
        error = "Graphics adapter must be 'virtio' or 'std'.";
        return false;
    }

    if (!is_one_of(config.network_mode, {"user", "none"})) {
        error = "Network mode must be 'user' or 'none'.";
        return false;
    }

    error.clear();
    return true;
}

}
