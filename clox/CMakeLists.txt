cmake_minimum_required(VERSION 3.17)
project(clox)

add_subdirectory(dependencies)

enable_testing()
add_subdirectory(test)

set(LIBRAY_SOURCES
        common.h
        scanner.h scanner.c
        memory.h memory.c
        value.h value.c
        object.h object.c
        chunk.h chunk.c
        table.h table.c
        debug.h debug.c
        vm.h vm.c compiler.h
        compiler.c file.h file.c)
add_library(CloxLib ${LIBRAY_SOURCES})
target_compile_options(CloxLib PRIVATE -Wall -Wextra -pedantic -Werror)
target_include_directories(CloxLib
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        )

add_executable(clox main.c)
target_link_libraries(clox CloxLib)