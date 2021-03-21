#include "catch2/catch.hpp"

extern "C" {
#include "file.h"
#include "vm.h"
}

TEST_CASE("Print Tests","[vm]") {
    const std::string printTests[] =
            {
//                    "break-while",
                    "empty",
                    "print",
                    "blocks",
                    "globalVars",
                    "var",
                    "scopes",
                    "expression",
                    "breakfast",
                    "fib35",
                    "outside",
                    "simple-upvalue",
                    "outer",
                    "dynamic-scope",
                    "closure",
                    "upvalue",
                    "flattening",
                    "makeClosure",
                    "devious",
                    "upvalue-disassembly"
            };
    const std::string printTestDir = "/Users/kja/repos/crafting-interpreters/clox/test/testData/vm/print/";

    for (const auto &testName : printTests) {
        DYNAMIC_SECTION(testName) {
            const std::string sourcePath = printTestDir + testName + ".lox";
            const std::string expectationsPath = sourcePath + ".out";

            FILE* tmp = tmpfile();

            VM vm;
            initVM(&vm);
            vm.outPipe = tmp;
            vm.errPipe = stdout;

            char *testSource = readFile(sourcePath.c_str());
            InterpretResult result = interpret(&vm, testSource);

            CHECK(result == INTERPRET_OK);

            char *actual = readFileHandle(tmp, "actual");
            char *expected = readFile(expectationsPath.c_str());
            CHECK_THAT(actual, Catch::Matchers::Equals(expected));

            free(expected);
            free(actual);
            fclose(tmp);
            free(testSource);
            freeVM(&vm);
        }
    }
}
