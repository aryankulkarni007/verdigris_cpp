cmake_minimum_required(VERSION 3.20)
project(Verdigris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)

# Source Discovery
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(verdigris ${SOURCES})

# This is the "Magic" line:
target_include_directories(verdigris PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Optimization
target_precompile_headers(verdigris PUBLIC <vector> <memory> <iostream>)

set_target_properties(verdigris PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
