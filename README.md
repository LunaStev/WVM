# WVM - Wave Virtual Machine Manager

WVM (Wave Virtual Machine Manager) is a lightweight CLI tool designed to simplify the management of QEMU virtual machines. It provides an intuitive interface for initializing, configuring, and running VMs using XML-based configurations.

## Features

- **Project-based VM Management**: Initialize a VM configuration within a directory.
- **Simplified CLI**: Easy-to-use commands for common VM operations.
- **XML Configuration**: Store VM settings in a readable `wvm.xml` file.
- **Automatic QEMU Command Generation**: Build complex QEMU commands automatically based on your configuration.
- **KVM Support**: Automatically enables KVM acceleration if `/dev/kvm` is available.

## Prerequisites

To build and run WVM, you need:

- **CMake** (version 3.20 or higher)
- **C++20 Compiler** (e.g., GCC 10+, Clang 10+)
- **pugixml** library
- **QEMU** (specifically the `qemu-system-*` binaries and `qemu-img` for disk management)

## Building

1. Clone the repository:
   ```bash
   git clone https://github.com/LunaStev/wvm.git
   cd wvm
   ```

2. Create a build directory and compile:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

3. (Optional) Install the binary:
   ```bash
   sudo make install
   ```

## Usage

### 1. Initialize a new VM project
Create a `wvm.xml` with default settings in the current directory.
```bash
wvm init
```

### 2. Configure VM parameters
Set the disk size, memory, and CPU cores. This command also creates the virtual disk image if it doesn't exist.
```bash
wvm set --size 64G --memory 4G --cores 4
```

### 3. Set installation source
Specify an ISO or disk image for installation.
```bash
wvm install . --iso /path/to/os-installer.iso
```

### 4. Run the VM
Launch the virtual machine using QEMU.
```bash
wvm run
```

## Configuration

The configuration is stored in `wvm.xml`. You can manually edit this file to fine-tune your VM settings:

```xml
<vm>
    <name>my-vm</name>
    <arch>x86_64</arch>
    <machine>q35</machine>
    <cpu>host</cpu>
    <memory>4G</memory>
    <cores>4</cores>
    <disk path="disk.qcow2" size="64G" format="qcow2"/>
    <boot mode="iso" path="os.iso"/>
    <display>gtk</display>
    <network mode="user"/>
</vm>
```

## License

This project is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**. See the [LICENSE](LICENSE) file for details.
