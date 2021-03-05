
add_executable(CloxTest
        tests-main.cpp
        tests-common.cpp
        tests-scanner.cpp
        tests-value.cpp tests-compiler.cpp)
target_link_libraries(CloxTest CloxLib Catch2::Catch2)
