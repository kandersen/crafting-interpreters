cmake_minimum_required(VERSION 3.17)
project(clox)

add_subdirectory(dependencies)

enable_testing()
add_subdirectory(test)

set(LIBRAY_SOURCES
        common.h
        scanner.h
        scanner.c
        )
add_library(CloxLib ${LIBRAY_SOURCES})
target_include_directories(CloxLib
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        )

add_executable(clox main.c)
target_link_libraries(clox CloxLib)