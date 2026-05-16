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
)

target_link_libraries(wvm PRIVATE pugixml::pugixml)