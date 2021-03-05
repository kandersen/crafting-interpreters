//
// Created by Kristoffer Just Arndal Andersen on 06/03/2021.
//
#include "catch2/catch.hpp"
extern "C" {
#include <compiler.h>
#include "object.h"
#include "memory.h"
}
TEST_CASE("Basic Testcase","[compiler]") {
    Obj* root = NULL;
    ObjFunction* res = compile(&root, "print(2+2);");
    REQUIRE(root == &res->obj);
    freeObjects(root);
}