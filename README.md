# WVM Workstation 2

WVM is a native Qt desktop workstation for local QEMU/KVM virtual machines. Version 2 replaces the CLI-driven GUI with a desktop application that manages QEMU processes, QMP commands, disks, and guest consoles directly. It does not require the v1 CLI to be installed.

The existing release is preserved at the annotated **`v1`** tag (`b8eefbd`). GUI development continues on **`v2`**. Version 2 is a development baseline, not a claim of VMware feature parity.

## Desktop workflow

Launch `wvm` from the applications menu or terminal. `wvm-gui` is retained as a compatibility launcher. The interface uses standard Qt menus, a toolbar, VM library tree, details and console tabs, and a dockable task log.

- **Persistent VM library:** keep multiple VMs, filter by name, view current state and reopen the same selection after restarting the app.
- **New VM wizard:** choose a guest family, name, location, CPU, memory, disk size, installer ISO, or existing disk image.
- **Guest profiles:** Linux/BSD use VirtIO devices; Windows/Other use compatible storage, VGA, and Intel E1000 networking. Hardware can be changed while powered off.
- **Disk import:** QEMU-supported images are converted to a separate QCOW2 copy without changing the source. Source snapshot histories are not copied.
- **Embedded console:** display and control the guest in the Qt app, including keyboard, pointer, full-screen mode, and Ctrl+Alt+Delete. Press Ctrl+Alt to release keyboard input.
- **Lifecycle controls:** power on, request guest shutdown, pause/resume, reset, and power off through QMP. Multiple VMs can run independently.
- **Settings:** edit CPU, memory, storage controller, graphics, NAT/disconnected networking, installation ISO, and notes.
- **Disk snapshots:** create, list, restore, and delete internal QCOW2 snapshots while powered off. These preserve disk contents, not guest memory.
- **Full clones:** create an independent QCOW2 disk and configuration from a powered-off VM.
- **Disk expansion:** increase virtual capacity, with actual disk size checked first; shrinking is rejected. Expand the partition/filesystem inside the guest afterwards.
- **v1 import:** File → Open Virtual Machine accepts an existing `wvm.xml` without moving or copying its disk.

Removing a VM from the library preserves its files. Closing the application with running VMs requires an explicit decision to power them off; disk operations must finish first.

## Acceleration and local connections

Native guests require KVM. WVM automatically prepares modules and user access through a narrow host-setup helper and the system administrator authorization dialog. When CPU virtualization is not exposed, the app explains the UEFI setting or outer-host nested virtualization requirement. It does not silently fall back to a slow software VM.

Compatible VirtIO guests automatically use `virtio-vga-gl` and EGL headless rendering when an accessible GPU render node is present. Otherwise WVM uses the 2D graphics path. Actual 3D support depends on the host drivers, QEMU build, and guest driver.

The integrated console uses the [RFB protocol](https://www.rfc-editor.org/rfc/rfc6143) on a private local Unix socket. No VNC TCP port is exposed. QMP uses a separate private socket, and each VM holds a runtime lock to prevent duplicate desktop instances. QEMU is launched with an argument vector without shell evaluation.

Library metadata lives under Qt's application-data directory, normally `~/.local/share/LunaStev/WVM/library.json`. VM configuration and disks remain in their VM directories. Runtime sockets live under the user runtime directory and use short hashed names, so long VM directory names do not exceed Unix socket limits.

## Build and install

The currently implemented host target is Linux x86_64. Guest profiles cover Linux, Windows, BSD, and other QEMU-compatible x86 operating systems; this is not yet a cross-platform host application.

Build dependencies: CMake 3.20+, a C++20 compiler, pugixml, Qt 6.2+ Widgets/Network, and Qt Test when testing is enabled. Runtime dependencies include QEMU system binaries, `qemu-img`, `kmod`, `acl`, and PolicyKit.

```bash
cmake -S . -B build/v2 -DCMAKE_BUILD_TYPE=Release
cmake --build build/v2 --parallel
ctest --test-dir build/v2 --output-on-failure
./build/v2/wvm
```

`scripts/install.sh` builds, tests, and installs under `/usr/local`. Set `WVM_INSTALL_PREFIX` for another prefix. Installed components are:

- `bin/wvm`: primary desktop application.
- `bin/wvm-gui`: compatibility launcher.
- `libexec/wvm/wvm-host-setup`: local KVM setup helper.
- Desktop launcher and application icon.

`scripts/package.sh` produces `.tar.gz` and available host-native `.deb`/`.rpm` packages in `build/package`. Binary packages should be built on the intended target distribution because Qt and C++ runtime versions differ.

The v1 CLI is optional for compatibility and automation:

```bash
cmake -S . -B build/v2 -DWVM_BUILD_LEGACY_CLI=ON
cmake --build build/v2 --parallel
./build/v2/wvm-cli doctor .
```

## Verification

Tests cover legacy XML compatibility and QEMU argument generation; persistent library and settings; real QCOW2 creation, snapshot content restoration, deletion, cloning, and preservation of removed VM files; plus an actual QEMU process with embedded framebuffer reception and QMP pause/resume/quit. The integration test explicitly uses TCG to run in development environments without KVM. The desktop app still requires KVM.

QEMU socket tests require an environment that allows local Unix socket creation. `qemu-img` and `qemu-system-x86_64` are required for the corresponding integration tests.

## Remaining workstation work

The current implementation uses legacy BIOS boot. UEFI firmware, Secure Boot and virtual TPM integration are not yet implemented, so do not treat Windows 11 installation as supported. USB/PCI/GPU passthrough, bridged/host-only networks, shared folders/clipboard, audio forwarding, saved-memory suspend and live snapshots, multi-disk editing, and remote hosts remain future work. The embedded console currently uses raw/CopyRect framebuffer updates, not a compressed remote-desktop transport.

QEMU remains the virtualization engine. WVM builds on the official [QEMU invocation](https://www.qemu.org/docs/master/system/invocation.html), [QMP](https://www.qemu.org/docs/master/interop/qmp-spec.html), and [qemu-img](https://www.qemu.org/docs/master/tools/qemu-img.html) interfaces rather than vendoring QEMU source.

## License

[Mozilla Public License 2.0](LICENSE).
