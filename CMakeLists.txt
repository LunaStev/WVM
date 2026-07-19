cmake_minimum_required(VERSION 3.20)

project(WVM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml REQUIRED)

add_executable(wvm
    src/main.cpp
    src/Commands.cpp
    src/VMConfig.cpp
    src/QemuCommandBuilder.cpp
    src/ProcessRunner.cpp
)

install(
    TARGETS wvm
    RUNTIME DESTINATION bin
)

target_link_libraries(wvm PRIVATE pugixml::pugixml)

include(CTest)

if(BUILD_TESTING)
    add_executable(wvm_tests
        tests/WvmTests.cpp
        src/VMConfig.cpp
        src/QemuCommandBuilder.cpp
        src/ProcessRunner.cpp
    )
    target_include_directories(wvm_tests PRIVATE src)
    target_link_libraries(wvm_tests PRIVATE pugixml::pugixml)

    add_test(NAME wvm_tests COMMAND wvm_tests)
endif()
