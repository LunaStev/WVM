# WVM - Wave Virtual Machine Manager

WVM (Wave Virtual Machine Manager) is an independent VM client built on top of QEMU/KVM. It manages project-local VM definitions, disks, installation media, and QEMU processes without routing commands through a shell.

## Features

- **Project-based VM Management**: Initialize a VM configuration within a directory.
- **Simplified CLI**: Easy-to-use commands for common VM operations.
- **XML Configuration**: Store VM settings in a readable `wvm.xml` file.
- **Automatic QEMU Command Generation**: Build complex QEMU commands automatically based on your configuration.
- **Automatic KVM Setup**: Loads KVM modules and grants the desktop user device access on first launch through the system authorization dialog.
- **Safe Process Execution**: Passes arguments directly to QEMU instead of evaluating shell command strings.
- **Dry Runs**: Inspect the exact QEMU invocation without starting or modifying a VM.
- **QMP Lifecycle Control**: Query, shut down, force-stop, and reset a running QEMU VM through its management socket.
- **Qt Desktop Client**: Create, install, start, stop, and inspect VMs from a focused single-window interface.
- **Native Performance Defaults**: Requires KVM for native guests and automatically uses VirtIO block, networking, and graphics devices.

## Prerequisites

To build and run WVM, you need:

- **CMake** (version 3.20 or higher)
- **C++20 Compiler** (e.g., GCC 10+, Clang 10+)
- **pugixml** library
- **Qt 6.2+ Widgets** development files for the desktop client
- **QEMU** (specifically the `qemu-system-*` binaries and `qemu-img` for disk management)

## Building

1. Clone the repository:
   ```bash
   git clone https://github.com/LunaStev/wvm.git
   cd wvm
   ```

2. Create a build directory and compile:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   ```

3. (Optional) Install the binary:
   ```bash
   sudo cmake --install build
   ```

## Usage

The desktop client is the primary interface:

```bash
wvm-gui
```

Choose or create a VM directory, set its resources, select an installation ISO, and use the Start, Restart, and Shut down buttons. The CLI remains available for headless systems and automation.

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

WVM applies KVM, VirtIO block, VirtIO networking, and VirtIO graphics automatically. If a writable GPU render node is available, it enables VirtIO GL and the GTK/SDL OpenGL display path; otherwise it uses VirtIO 2D. These are managed defaults rather than user-facing tuning options.

### Performance check

```bash
wvm doctor .
```

For a native guest, WVM prepares the KVM kernel modules and `/dev/kvm` access automatically when the VM is powered on. The operating system may show one administrator authorization prompt. If the CPU virtualization flag itself is missing, WVM gives the exact AMD SVM or Intel VT-x firmware setting to enable; firmware is the only layer an application cannot change. WVM refuses to silently launch a slow native guest through TCG.

`wvm host-setup` can be used to perform the same automatic host preparation before starting a VM.

### 3. Set installation source
Specify an ISO or disk image for installation. ISO media is used for the first boot only; an installer reboot returns to the VM disk, and a successfully completed QEMU session switches future launches to disk mode.
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

If QEMU was killed and left a stale `.wvm/qmp.sock`, first confirm that no VM process is running and then use `wvm run . --recover` to remove only that stale runtime socket.

### 5. Control a running VM

While `wvm run` owns the foreground QEMU process, another terminal can control it through QMP:

```bash
wvm status .
wvm reboot .
wvm stop .
wvm stop . --force
```

`stop` requests an ACPI shutdown from the guest. `--force` sends QEMU's immediate `quit` command.

## Configuration

The configuration is stored in `wvm.xml`. You can manually edit this file to fine-tune your VM settings:

```xml
<wvm version="1">
    <machine>
        <name>my-vm</name>
        <arch>x86_64</arch>
        <type>q35</type>
        <cpu>host</cpu>
        <acceleration>kvm</acceleration>
        <memory>4G</memory>
        <cores>4</cores>
    </machine>
    <disk>
        <path>disk.qcow2</path>
        <size>64G</size>
        <format>qcow2</format>
        <interface>virtio</interface>
    </disk>
    <boot>
        <mode>iso</mode>
        <path>os.iso</path>
    </boot>
    <display>
        <type>gtk</type>
        <graphics>virtio</graphics>
    </display>
    <network>
        <mode>user</mode>
    </network>
</wvm>
```

Performance-related values are written by WVM and normally do not need manual editing. Supported disk formats are `qcow2` and `raw`. Boot modes are `none`, `disk`, `iso`, and `img`; network modes are `user` and `none`.

## Installation and packages

Build, test, and install under `/usr/local`:

```bash
scripts/install.sh
```

Set `WVM_INSTALL_PREFIX` to choose another prefix. To generate the packages supported by the host (`.tar.gz`, `.deb`, and/or `.rpm`):

```bash
scripts/package.sh
```

The packages include `wvm`, `wvm-gui`, the desktop launcher, and the scalable application icon.

## Testing

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Project Direction

WVM uses QEMU/KVM as its virtualization engine and aims to provide its own lifecycle, storage, networking, snapshot, and desktop client experience. QEMU remains the low-level device and CPU emulator; WVM owns the safer and simpler user-facing workflow.

QEMU is not vendored or evaluated through a shell. WVM launches the installed `qemu-system-*` executable with an argument vector and exposes a project-local `.wvm/qmp.sock` using the official [QEMU Machine Protocol](https://www.qemu.org/docs/master/interop/qmp-spec.html). This keeps QEMU replaceable while giving WVM direct programmatic control over each VM.

## License

This project is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**. See the [LICENSE](LICENSE) file for details.
