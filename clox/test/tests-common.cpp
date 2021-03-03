#include "catch2/catch.hpp"

extern "C" {
#include "common.h"
}

TEST_CASE("Max unsigned 8-bit Int","[common]") {
    REQUIRE(UINT8_COUNT == 256);
}
