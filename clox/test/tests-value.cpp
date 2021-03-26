#include "catch2/catch.hpp"

extern "C" {
#include "value.h"
#include "memory.h"
}

TEST_CASE("Linked Value List","[value]") {
    MemoryManager* nullCollector = nullptr;

    ValueArray array;
    initValueArray(&array);

    SECTION("Free Empty") {
        REQUIRE(array.count == 0);
        REQUIRE_NOTHROW(freeValueArray(nullCollector, &array));
    }

    SECTION("A couple of elements") {
        Value five = NUMBER_VAL(5);
        for(int i = 0; i < 3; i++) {
            writeValueArray(nullCollector, &array, five);
        }
        SECTION("Count is right") {
            REQUIRE(array.count == 3);
        }
        SECTION("Free succeeds") {
            REQUIRE_NOTHROW(freeValueArray(nullCollector, &array));
        }
    }

    freeValueArray(nullCollector, &array);
}
