#include "catch2/catch.hpp"

extern "C" {
#include "scanner.h"
}

TEST_CASE("Basic Lexing", "[scanner]") {
    Scanner scanner;
    initScanner(&scanner, "Hej + dig");
    Token token;
    token = scanToken(&scanner);
    REQUIRE(token.type == TOKEN_IDENTIFIER);
    token = scanToken(&scanner);
    REQUIRE(token.type == TOKEN_PLUS);
    token = scanToken(&scanner);
    REQUIRE(token.type == TOKEN_IDENTIFIER);
    token = scanToken(&scanner);
    REQUIRE(token.type == TOKEN_EOF);
    scanToken(&scanner);
    scanToken(&scanner);
    scanToken(&scanner);
    token = scanToken(&scanner);
    REQUIRE(token.type == TOKEN_EOF);
}