cmake_minimum_required(VERSION 3.20)

project(WVM VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml REQUIRED)
option(WVM_BUILD_GUI "Build the Qt desktop client" ON)

add_executable(wvm
    src/main.cpp
    src/Commands.cpp
    src/VMConfig.cpp
    src/QemuCommandBuilder.cpp
    src/ProcessRunner.cpp
    src/QmpClient.cpp
    src/HostCapabilities.cpp
)

install(
    TARGETS wvm
    RUNTIME DESTINATION bin
)

target_link_libraries(wvm PRIVATE pugixml::pugixml)

if(WVM_BUILD_GUI)
    find_package(Qt6 6.5 COMPONENTS Widgets REQUIRED)

    add_executable(wvm-gui
        src/gui/main.cpp
        src/gui/MainWindow.cpp
        src/gui/MainWindow.hpp
    )
    target_link_libraries(wvm-gui PRIVATE Qt6::Widgets)

    install(
        TARGETS wvm-gui
        RUNTIME DESTINATION bin
    )
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
endif()

set(CPACK_PACKAGE_NAME "wvm")
set(CPACK_PACKAGE_VENDOR "LunaStev")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Wave Virtual Machine Manager for QEMU/KVM")
set(CPACK_PACKAGE_DESCRIPTION "A focused Qt desktop and CLI client for hardware-accelerated QEMU/KVM virtual machines.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "luna@lunastev.org")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qemu-system-x86, qemu-utils, libqt6widgets6")
set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
endif()
set(CPACK_RPM_PACKAGE_LICENSE "MPL-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_REQUIRES "qemu-system-x86-core, qemu-img, qt6-qtbase-gui")

include(CPack)
