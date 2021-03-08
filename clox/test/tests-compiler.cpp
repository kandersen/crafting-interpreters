#include <libc.h>
#include "catch2/catch.hpp"

extern "C" {
#include "debug.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "file.h"
}

TEST_CASE("Compilation Error","[compiler]") {
    const std::string compilationErrorTests[] = {

    };
    for (const auto &testName : compilationErrorTests) {
        DYNAMIC_SECTION(testName) {
            CHECK(1 == 1);
        }
    }
}

TEST_CASE("Disassembly Dump Tests","[compiler]") {
    const std::string disassemblyDumpTests[] = {
            "empty",
            "print",
            "globalVar",
            "blocks"
    };
    const std::string disassemblyDumpTestDir = "/Users/kja/repos/crafting-interpreters/clox/test/testData/compiler/disassemblyDump/";

    for (const auto &testName : disassemblyDumpTests) {
        DYNAMIC_SECTION(testName) {
            const std::string sourcePath = disassemblyDumpTestDir + testName + ".lox";
            const std::string expectationsPath = sourcePath + ".assembly";

            char *testSource = readFile(sourcePath.c_str());
            Obj *root = nullptr;
            Table strings;
            initTable(&strings);
            Globals globals;
            initGlobals(&globals);
            ObjFunction *compilationResult = compile(&strings, &globals, &root, testSource);

            FILE *tmp = tmpfile();
            disassembleChunk(tmp, &compilationResult->chunk,compilationResult->name != nullptr ? compilationResult->name->chars : "<script>");

            char *actual = readFileHandle(tmp, "actual");
            char *expected = readFile(expectationsPath.c_str());
            CHECK_THAT(actual, Catch::Matchers::Equals(expected));

            free(expected);
            free(actual);
            fclose(tmp);
            freeGlobals(&globals);
            freeObjects(root);
            free(testSource);
        }
    }
}
