#include "catch2/catch.hpp"

extern "C" {
#include "scanner.h"
}

TEST_CASE("Basic Lexing", "[scanner]") {
    Scanner scanner;
    initScanner(&scanner, "Hej + dig");
    Token token;

    SECTION("Identifier Token") {
        token = scanToken(&scanner);
        REQUIRE(token.type == TOKEN_IDENTIFIER);
        REQUIRE_THAT(token.start, Catch::Matchers::StartsWith("Hej"));
        token = scanToken(&scanner);
        REQUIRE(token.type == TOKEN_PLUS);
        token = scanToken(&scanner);
        REQUIRE(token.type == TOKEN_IDENTIFIER);
    }

    SECTION("EOF Behavior") {
        for (int i = 0; i < 10; i++) {
            token = scanToken(&scanner);
        }
        REQUIRE(token.type == TOKEN_EOF);
    }
}