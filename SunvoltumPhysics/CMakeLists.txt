cmake_minimum_required(VERSION 3.8)

project(SunvoltumPhysics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE PHYSICS_HEADERS
    "include/SunvoltumPhysics/*.h"
)

file(GLOB_RECURSE PHYSICS_SOURCES
    "src/*.cpp"
)

list(APPEND PHYSICS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collision/NarrowPhase/GJK_EPA.cpp"
)
list(REMOVE_DUPLICATES PHYSICS_SOURCES)

add_library(SunvoltumPhysics STATIC
    ${PHYSICS_HEADERS}
    ${PHYSICS_SOURCES}
)

target_include_directories(SunvoltumPhysics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if (MSVC)
    target_compile_options(SunvoltumPhysics PRIVATE /W4 /permissive-)
endif()
