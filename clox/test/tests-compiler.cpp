#include "catch2/catch.hpp"

extern "C" {
#include "debug.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "file.h"
}

TEST_CASE("Disassembly Dump Tests","[compiler]") {
    const std::string disassemblyDumpTests[] =
            {
                    "empty",
                    "print"
            };
    const std::string disassemblyDumpTestDir = "/Users/kja/repos/crafting-interpreters/clox/test/testData/compiler/disassemblyDumpTest/";

    for (const auto &testName : disassemblyDumpTests) {
        DYNAMIC_SECTION(testName) {
            const std::string sourcePath = disassemblyDumpTestDir + testName + ".lox";
            const std::string expectationsPath = sourcePath + ".assembly";

            char *testSource = readFile(sourcePath.c_str());
            Obj *root = nullptr;
            ObjFunction *compilationResult = compile(&root, testSource);

            FILE *tmp = tmpfile();
            disassembleChunk(tmp, &compilationResult->chunk,
                             compilationResult->name != nullptr ? compilationResult->name->chars : "<script>");

            char *actual = readFileHandle(tmp, "actual");
            char *expected = readFile(expectationsPath.c_str());
            CHECK_THAT(actual, Catch::Matchers::Equals(expected));

            free(expected);
            free(actual);
            fclose(tmp);
            freeObjects(root);
            free(testSource);
        }
    }
}