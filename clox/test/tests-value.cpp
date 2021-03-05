#include "catch2/catch.hpp"

extern "C" {
#include "value.h"
}

TEST_CASE("Linked Value List","[value]") {
    ValueArray array;
    initValueArray(&array);
    SECTION("Free Empty") {
        REQUIRE(array.count == 0);
        REQUIRE_NOTHROW(freeValueArray(&array));
    }

    SECTION("A couple of elements") {
        Value five = NUMBER_VAL(5);
        for(int i = 0; i < 3; i++) {
            writeValueArray(&array, five);
        }
        SECTION("Count is right") {
            REQUIRE(array.count == 3);
        }
        SECTION("Free succeeds") {
            REQUIRE_NOTHROW(freeValueArray(&array));
        }
    }

    freeValueArray(&array);
}
