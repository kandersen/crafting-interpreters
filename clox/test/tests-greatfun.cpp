#include "catch2/catch.hpp"

extern "C" {
  #include <lib.h>
}

TEST_CASE("foo", "[littleFun]") {
    REQUIRE(greatFun() == 42);
}

