# RERev2 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(RERev2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(REREV2_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(RERev2 WIN32 ${REREV2_SOURCES})
else()
    add_executable(RERev2 ${REREV2_SOURCES})
endif()

rexglue_setup_target(RERev2)
