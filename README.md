# WVM - Wave Virtual Machine Manager

WVM (Wave Virtual Machine Manager) is an independent VM client built on top of QEMU/KVM. It manages project-local VM definitions, disks, installation media, and QEMU processes without routing commands through a shell.

## Features

- **Project-based VM Management**: Initialize a VM configuration within a directory.
- **Simplified CLI**: Easy-to-use commands for common VM operations.
- **XML Configuration**: Store VM settings in a readable `wvm.xml` file.
- **Automatic QEMU Command Generation**: Build complex QEMU commands automatically based on your configuration.
- **KVM Support**: Enables KVM only for a compatible native architecture when `/dev/kvm` is accessible.
- **Safe Process Execution**: Passes arguments directly to QEMU instead of evaluating shell command strings.
- **Dry Runs**: Inspect the exact QEMU invocation without starting or modifying a VM.

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

Boot from the system disk or temporarily preview another source:

```bash
wvm run . --disk
wvm run . --iso /path/to/os-installer.iso --dry-run
wvm run . --img /path/to/boot.img --dry-run
```

## Configuration

The configuration is stored in `wvm.xml`. You can manually edit this file to fine-tune your VM settings:

```xml
<wvm version="1">
    <machine>
        <name>my-vm</name>
        <arch>x86_64</arch>
        <type>q35</type>
        <cpu>host</cpu>
        <memory>4G</memory>
        <cores>4</cores>
    </machine>
    <disk>
        <path>disk.qcow2</path>
        <size>64G</size>
        <format>qcow2</format>
    </disk>
    <boot>
        <mode>iso</mode>
        <path>os.iso</path>
    </boot>
    <display>
        <type>gtk</type>
    </display>
    <network>
        <mode>user</mode>
    </network>
</wvm>
```

Supported disk formats are `qcow2` and `raw`. Boot modes are `none`, `disk`, `iso`, and `img`; network modes are `user` and `none`.

## Testing

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Project Direction

WVM uses QEMU/KVM as its virtualization engine and aims to provide its own lifecycle, storage, networking, snapshot, and desktop client experience. QEMU remains the low-level device and CPU emulator; WVM owns the safer and simpler user-facing workflow.

## License

This project is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**. See the [LICENSE](LICENSE) file for details.
