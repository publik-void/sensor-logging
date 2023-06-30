cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(sensor-logging)
add_executable(sensor-logging src/main.cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3")
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0 -ggdb3") # For `gcc`
#string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0 -glldb") # For `clang`

# Compiler output
add_compile_options(-W -Wall -Wextra)
string(APPEND CMAKE_CXX_FLAGS " -fdiagnostics-color=always") # For `gcc`
#string(APPEND CMAKE_CXX_FLAGS " -fcolor-diagnostics") # For `clang`

# This should roughly be the CMake way of doing `gcc -pthread`, as is needed for
# `pigpio`. See `https://stackoverflow.com/a/39547577`.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(sensor-logging Threads::Threads)

# Link to `pigpio`, specifically the daemon socket interface variant
target_link_libraries(sensor-logging pigpiod_if2)

# Create `platform.hpp` during CMake configuration time
execute_process(COMMAND ${PROJECT_SOURCE_DIR}/script/print-platform-header.sh
  OUTPUT_FILE ${PROJECT_SOURCE_DIR}/src/platform.hpp)

