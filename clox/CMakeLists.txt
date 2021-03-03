cmake_minimum_required(VERSION 3.17)
project(clox C)

set(CMAKE_C_STANDARD 11)

set(SOURCES main.c)

add_executable(clox ${SOURCES})