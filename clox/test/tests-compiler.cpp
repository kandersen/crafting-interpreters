#include "catch2/catch.hpp"

extern "C" {
#include "debug.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "file.h"
}

void nullCollectorStackPush(void* stack, void* data) {
    return;
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
//            "empty",
//            "print",
//            "globalVar",
//            "blocks",
//            "upvalue-disassembly",
//            "simple-upvalue",
//            "upvalue-assignment",
            "brioche"
    };
    const std::string disassemblyDumpTestDir = "/Users/kja/repos/crafting-interpreters/clox/test/testData/compiler/disassemblyDump/";

    for (const auto &testName : disassemblyDumpTests) {
        DYNAMIC_SECTION(testName) {
            const std::string sourcePath = disassemblyDumpTestDir + testName + ".lox";
            const std::string expectationsPath = sourcePath + ".assembly";

            char *testSource = readFile(sourcePath.c_str());
            Table strings;
            initTable(&strings);
            Globals globals;
            initGlobals(&globals);

            MemoryManager nullCollector;
            initMemoryManager(&nullCollector);
            nullCollector.pushStack = nullCollectorStackPush;
            nullCollector.popStack = nullMemoryComponentFn;

            ObjFunction* compilationResult = compile(&nullCollector, &strings, &globals, testSource);
            REQUIRE(compilationResult != NULL);
            FILE *tmp = tmpfile();

            fprintf(tmp, "===| ASSEMBLY |===\n");
            disassembleChunk(tmp, &compilationResult->chunk, "<script>");
            fprintf(tmp, "\n===| INTERNED STRINGS |===\n");
            dumpInternedStrings(tmp, &strings);
            fprintf(tmp, "\n===| GLOBALS |===\n");
            dumpGlobals(tmp, &globals);

            char *actual = readFileHandle(tmp, "actual");
            char *expected = readFile(expectationsPath.c_str());
            CHECK_THAT(actual, Catch::Matchers::Equals(expected));

            free(expected);
            free(actual);
            fclose(tmp);
            freeGlobals(&nullCollector, &globals);
            freeMemoryManager(&nullCollector);
            free(testSource);
        }
    }
}
