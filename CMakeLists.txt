cmake_minimum_required(VERSION 3.16)
project(NetworkGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── yaml-cpp via FetchContent (Commit 5.1) ──
# The plan targets 0.8.0 but that release is missing #include <cstdint> in
# emitterutils.cpp, causing compile errors with GCC 16. Using master which
# contains the fix. Swap back to GIT_TAG 0.8.0 once a patched release ships.
include(FetchContent)
# Allow yaml-cpp's own CMakeLists (cmake_minimum_required VERSION 2.8) to
# configure without error under CMake 4.x.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# ── Gateway executable ──
add_executable(gateway
    src/main.cpp
    src/core/socket_utils.cpp
)

target_include_directories(gateway PRIVATE include src)
target_compile_options(gateway PRIVATE -Wall -Wextra)
target_link_libraries(gateway PRIVATE yaml-cpp::yaml-cpp)
