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
            "blocks",
            "upvalue-disassembly",
            "simple-upvalue",
            "upvalue-assignment"
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
            REQUIRE(compilationResult != NULL);
            FILE *tmp = tmpfile();

            fprintf(tmp, "===| ASSEMBLY |===\n");
            Obj* nextObj = root;
            while (nextObj != nullptr) {
                if (nextObj->type == OBJ_FUNCTION) {
                    auto* compiledFunction = (ObjFunction*)nextObj;
                    disassembleChunk(tmp, &compiledFunction->chunk, compiledFunction->name != nullptr ? compiledFunction->name->chars : "<script>");
                    fprintf(tmp,"-- Constants --\n");
                    dumpConstants(tmp, &compiledFunction->chunk);
                    fprintf(tmp,"\n");
                }
                nextObj = nextObj->next;
            }

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
            freeGlobals(&globals);
            freeObjects(root);
            free(testSource);
        }
    }
}
