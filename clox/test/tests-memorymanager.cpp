#include "catch2/catch.hpp"

extern "C" {
#include "memory.h"
}

void markRoot(void* data) {
    bool* root = (bool*)data;
    *root = true;
}

TEST_CASE("Memory Manager","[memorymanager]") {
    MemoryManager mm;
    initMemoryManager(&mm);

    bool rootIsMarked = false;

    MemoryComponent vmComponent;
    vmComponent.data = &rootIsMarked;
    vmComponent.markRoots = markRoot;
    vmComponent.handleWeakReferences = nullMemoryComponentFn;
    vmComponent.next = mm.memoryComponents;
    mm.memoryComponents = &vmComponent;

    collectGarbage(&mm);

    freeMemoryManager(&mm);
    REQUIRE(rootIsMarked);
}
