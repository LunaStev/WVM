#include "VMConfig.hpp"

#include <pugixml.hpp>

#include <filesystem>
#include <fstream>
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
    machine.append_child("memory").text().set(config.memory.c_str());
    machine.append_child("cores").text().set(config.cores);

    auto disk = root.append_child("disk");
    disk.append_child("path").text().set(config.disk_path.c_str());
    disk.append_child("size").text().set(config.disk_size.c_str());
    disk.append_child("format").text().set(config.disk_format.c_str());

    auto boot = root.append_child("boot");
    boot.append_child("mode").text().set(config.boot_mode.c_str());
    boot.append_child("path").text().set(config.boot_path.c_str());

    auto display = root.append_child("display");
    display.append_child("type").text().set(config.display.c_str());

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
    config.memory = text_or(machine.child("memory"), "4G");
    config.cores = int_or(machine.child("cores"), 4);

    config.disk_path = text_or(disk.child("path"), "disk.qcow2");
    config.disk_size = text_or(disk.child("size"), "64G");
    config.disk_format = text_or(disk.child("format"), "qcow2");

    config.boot_mode = text_or(boot.child("mode"), "none");
    config.boot_path = text_or(boot.child("path"), "");

    config.display = text_or(display.child("type"), "gtk");
    config.network_mode = text_or(network.child("mode"), "user");

    return true;
}

}