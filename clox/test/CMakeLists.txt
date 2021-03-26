
add_executable(CloxTest
        tests-main.cpp
        tests-common.cpp
        tests-scanner.cpp
        tests-value.cpp
        tests-compiler.cpp
        tests-vm.cpp tests-memorymanager.cpp)
target_link_libraries(CloxTest CloxLib Catch2::Catch2)
