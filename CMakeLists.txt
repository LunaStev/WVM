cmake_minimum_required(VERSION 3.20)

project(WVM VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml REQUIRED)
option(WVM_BUILD_GUI "Build the Qt desktop client" ON)
option(WVM_BUILD_LEGACY_CLI "Build the optional v1 command-line client" OFF)

add_library(wvm_core STATIC
    src/Commands.cpp
    src/VMConfig.cpp
    src/QemuCommandBuilder.cpp
    src/ProcessRunner.cpp
    src/QmpClient.cpp
    src/HostCapabilities.cpp
)
target_include_directories(wvm_core PUBLIC src)
target_link_libraries(wvm_core PUBLIC pugixml::pugixml)
add_executable(wvm-host-setup src/HostSetupMain.cpp)
target_link_libraries(wvm-host-setup PRIVATE wvm_core)
install(TARGETS wvm-host-setup RUNTIME DESTINATION libexec/wvm)
if(WVM_BUILD_LEGACY_CLI)
    add_executable(wvm-cli src/main.cpp)
    target_link_libraries(wvm-cli PRIVATE wvm_core)
    install(TARGETS wvm-cli RUNTIME DESTINATION bin)
endif()

if(WVM_BUILD_GUI)
    find_package(Qt6 6.2 COMPONENTS Widgets Network REQUIRED)
    set(CMAKE_AUTOMOC ON)

    add_library(wvm_desktop STATIC
        src/gui/VmManager.cpp
        src/gui/VmManager.hpp
        src/gui/ConsoleView.cpp
        src/gui/ConsoleView.hpp
        src/gui/VmDialogs.cpp
        src/gui/VmDialogs.hpp
    )
    target_link_libraries(wvm_desktop PUBLIC wvm_core Qt6::Widgets Qt6::Network)
    target_include_directories(wvm_desktop PUBLIC src/gui)

    add_executable(wvm-gui
        src/gui/main.cpp
        src/gui/MainWindow.cpp
        src/gui/MainWindow.hpp
    )
    set_target_properties(wvm-gui PROPERTIES OUTPUT_NAME wvm)
    target_link_libraries(wvm-gui PRIVATE wvm_desktop)

    install(
        TARGETS wvm-gui
        RUNTIME DESTINATION bin
    )
    install(PROGRAMS packaging/wvm-gui DESTINATION bin)
    install(
        FILES packaging/io.github.lunastev.WVM.desktop
        DESTINATION share/applications
    )
    install(
        FILES assets/wvm.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(wvm_tests
        tests/WvmTests.cpp
        src/VMConfig.cpp
        src/QemuCommandBuilder.cpp
        src/ProcessRunner.cpp
        src/QmpClient.cpp
    )
    target_include_directories(wvm_tests PRIVATE src)
    target_link_libraries(wvm_tests PRIVATE pugixml::pugixml)

    add_test(NAME wvm_tests COMMAND wvm_tests)
    set_tests_properties(wvm_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    if(WVM_BUILD_GUI)
        find_package(Qt6 6.2 COMPONENTS Test REQUIRED)
        add_executable(wvm_desktop_tests tests/DesktopTests.cpp)
        target_link_libraries(wvm_desktop_tests PRIVATE wvm_desktop Qt6::Test)
        add_test(NAME wvm_desktop_tests COMMAND wvm_desktop_tests)
        set_tests_properties(wvm_desktop_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

set(CPACK_PACKAGE_NAME "wvm")
set(CPACK_PACKAGE_VENDOR "LunaStev")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Wave Virtual Machine Manager for QEMU/KVM")
set(CPACK_PACKAGE_DESCRIPTION "A native Qt desktop workstation with a persistent VM library, embedded console, snapshots and direct QEMU/KVM control.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "luna@lunastev.org")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qemu-system-x86, qemu-utils, libqt6widgets6, libqt6network6, libpugixml1v5, kmod, acl, pkexec")
set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
endif()
set(CPACK_RPM_PACKAGE_LICENSE "MPL-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_REQUIRES "qemu-system-x86-core, qemu-img, qt6-qtbase-gui, kmod, acl, polkit")

include(CPack)
