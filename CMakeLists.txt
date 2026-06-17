cmake_minimum_required(VERSION 3.14)
project(RegionHybrid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_CLI "Build the command line example" ON)

# Main library
add_library(region_hybrid STATIC
    region_hybrid.cpp
)
target_include_directories(region_hybrid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# CLI Example
if(BUILD_CLI)
    add_executable(region_hybrid_cli
        examples/cli.cpp
    )
    target_link_libraries(region_hybrid_cli PRIVATE region_hybrid)
endif()

message(STATUS "RegionHybrid CMake configured successfully!")
message(STATUS "To build: mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j")
