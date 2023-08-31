cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CMake FAQ, maybe this will come in handy:
# https://gitlab.kitware.com/cmake/community/-/wikis/FAQ

# NOTE: CMake is a mess as always. Some of the following properties have to be
# set before declaring a project, but some apparantly not, and then there are
# others that must be set afterwards, and all of this is not very transparent.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if (${CMAKE_GENERATOR} STREQUAL "Ninja Multi-Config")
  set(CMAKE_DEFAULT_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

# When using this with `target_link_libraries(<target> Threads::Threads)`, it
# should roughly be the CMake way of doing `gcc -pthread`, as is needed for
# `pigpio`. See `https://stackoverflow.com/a/39547577`.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

project(sensor-logging)

# NOTE: At least for the "Release" build type, it seems that appending an extra
# `-O3` to the flags somehow removes the automatic addition of `-DNDEBUG`. I
# reckon that I may just want to let the toolchain decide the release build
# optimization level by itself and leave the flags alone. It seems to result in
# `-O3` anyway. For the "Debug" build type, I want to use `-ggdb3`/`-glldb`,
# though, and there's no `-DNDEBUG` to override then anyway.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0" " -ggdb3")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # "Clang" or "AppleClang"
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0" " -glldb")
endif()

add_compile_options(-W -Wall -Wextra)

# For "Ninja" or "Ninja Multi-Config", colorized output has to be forced
if (${CMAKE_GENERATOR} MATCHES "Ninja")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(APPEND CMAKE_CXX_FLAGS " -fdiagnostics-color=always")
  elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # "Clang" or "AppleClang"
    string(APPEND CMAKE_CXX_FLAGS " -fcolor-diagnostics")
  endif()
endif()

add_subdirectory(include/DHTXXD)
add_subdirectory(include/_433D)
add_executable(sensor-logging src/main.cpp)

# Link to POSIX Threads
find_package(Threads REQUIRED)
target_link_libraries(sensor-logging Threads::Threads)

# Link to `pigpio`, specifically the daemon socket interface variant
target_link_libraries(sensor-logging pigpiod_if2)

# Make header files under `include/` available and link to the libraries
target_include_directories(sensor-logging PUBLIC include/DHTXXD)
target_link_libraries(sensor-logging DHTXXD)
target_include_directories(sensor-logging PUBLIC include/_433D)
target_link_libraries(sensor-logging _433D)

# Run code generation scripts whenever they or their inputs have changed
add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/machine.generated.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND python3 script/generate-machine-include.py
  DEPENDS script/generate-machine-include.py
  COMMENT "Running `generate-machine-include.py`…")
add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/sensors.generated.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND python3 script/generate-sensors-include.py
  DEPENDS script/sensors.json script/generate-sensors-include.py
  COMMENT "Running `generate-sensors-include.py`…")
add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/control.generated.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND python3 script/generate-control-include.py
  DEPENDS script/control-structs.json script/generate-control-include.py
  COMMENT "Running `generate-control-include.py`…")
add_custom_target(generate-machine-include ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/src/machine.generated.cpp)
add_custom_target(generate-sensors-include ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/src/sensors.generated.cpp)
add_custom_target(generate-control-include ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/src/control.generated.cpp)
add_dependencies(sensor-logging generate-machine-include)
add_dependencies(sensor-logging generate-sensors-include)
add_dependencies(sensor-logging generate-control-include)

