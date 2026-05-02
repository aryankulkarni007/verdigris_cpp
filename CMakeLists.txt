cmake_minimum_required(VERSION 3.20)
project(Verdigris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wmisleading-indentation -Wduplicated-cond -Wlogical-op)
endif()

# --- OS-Specific Setup ---
if(APPLE)
    message(STATUS "Build: macOS Native")
elseif(UNIX AND NOT APPLE)
    message(STATUS "Build: Linux / Docker")
    add_compile_options(-O0 -g -fno-inline)
endif()

# --- Sources and Targets ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(verdigris ${SOURCES})

target_include_directories(verdigris PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_precompile_headers(verdigris PUBLIC <vector> <memory> <iostream> <string_view>)

set_target_properties(verdigris PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
