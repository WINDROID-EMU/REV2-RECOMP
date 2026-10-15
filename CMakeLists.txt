# RERev2 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(RERev2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

if(CMAKE_CROSSCOMPILING AND TARGET rex::rexglue)
    set(_HOST_REXGLUE "${CMAKE_CURRENT_SOURCE_DIR}/../linux-amd64/bin/rexglue")
    if(EXISTS "${_HOST_REXGLUE}")
        set_target_properties(rex::rexglue PROPERTIES
            IMPORTED_LOCATION "${_HOST_REXGLUE}"
            IMPORTED_LOCATION_RELEASE "${_HOST_REXGLUE}"
            IMPORTED_LOCATION_DEBUG "${_HOST_REXGLUE}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${_HOST_REXGLUE}"
        )
    endif()
endif()

# Sources
set(REREV2_SOURCES
    src/main.cpp
    src/rev2_hooks.cpp
    src/rev2_trace.cpp
    src/rev2_gpu/rev2_gpu.cpp
)

if(WIN32)
    add_executable(RERev2 WIN32 ${REREV2_SOURCES})
else()
    add_executable(RERev2 ${REREV2_SOURCES})
endif()

rexglue_setup_target(RERev2 GPU_PLUGINS xenos)
