cmake_minimum_required(VERSION 3.13)
project(clox)

set(CMAKE_C_STANDARD 11)
set(GCC_COVERAGE_COMPILE_FLAGS "-Wall -Wextra -Wconversion")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )


file(GLOB SOURCES "*.c")

add_executable(clox ${SOURCES})
target_include_directories(clox PUBLIC .)
