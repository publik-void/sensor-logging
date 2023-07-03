cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# NOTE: CMake is a mess as always. Some of the following properties have to be
# set before declaring a project, but some apparantly not. I'm not sure which
# and why.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3")
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0 -ggdb3") # For `gcc`
#string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0 -glldb") # For `clang`

# Compiler output
add_compile_options(-W -Wall -Wextra)
string(APPEND CMAKE_CXX_FLAGS " -fdiagnostics-color=always") # For `gcc`
#string(APPEND CMAKE_CXX_FLAGS " -fcolor-diagnostics") # For `clang`

project(sensor-logging)
add_executable(sensor-logging src/main.cpp)

# This should roughly be the CMake way of doing `gcc -pthread`, as is needed for
# `pigpio`. See `https://stackoverflow.com/a/39547577`.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(sensor-logging Threads::Threads)

# Link to `pigpio`, specifically the daemon socket interface variant
target_link_libraries(sensor-logging pigpiod_if2)

# Run code generation scripts whenever they or their inputs have changed

add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/machine.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND python3 script/write-machine-include.py
  DEPENDS script/write-machine-include.py
  COMMENT "Running `write-machine-include.py`…")
add_custom_target(write-machine-include ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/src/machine.cpp)
add_dependencies(sensor-logging write-machine-include)

add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/sensors.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND python3 script/write-sensors-include.py
  DEPENDS script/sensors.json script/write-sensors-include.py
  COMMENT "Running `write-sensors-include.py`…")
add_custom_target(write-sensors-include ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/src/sensors.cpp)
add_dependencies(sensor-logging write-sensors-include)

