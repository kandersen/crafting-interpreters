
add_executable(CloxTest tests-main.cpp tests-greatfun.cpp)
target_link_libraries(CloxTest CloxLib Catch2::Catch2)
