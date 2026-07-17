cmake_minimum_required(VERSION 3.16)
project(NetworkGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Gateway executable ──
add_executable(gateway
    src/main.cpp
    src/core/socket_utils.cpp
)

target_include_directories(gateway PRIVATE include src)
target_compile_options(gateway PRIVATE -Wall -Wextra)
