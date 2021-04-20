#include "catch2/catch.hpp"

extern "C" {
#include "memory.h"
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
                    "upvalue-assignment",
                    "globalGet",
                    "makeClosure",
                    "devious",
                    "upvalue-disassembly",
                    "sibling-closure",
                    "loop-closure",
                    "for-loop",
                    "brioche",
                    "call-with-args",
                    "toast"
            };
    const std::string printTestDir = "/Users/kja/repos/crafting-interpreters/clox/test/testData/vm/print/";

    for (const auto &testName : printTests) {
        DYNAMIC_SECTION(testName) {
            const std::string sourcePath = printTestDir + testName + ".lox";
            const std::string expectationsPath = sourcePath + ".out";

            FILE* tmp = tmpfile();

            MemoryManager mm;
            initMemoryManager(&mm);

            VM vm;
            initVM(&vm);
            vm.mm = &mm;

            MemoryComponent vmComponent;
            vmComponent.data = &vm;
            vmComponent.markRoots = markVMRoots;
            vmComponent.handleWeakReferences = handleWeakVMReferences;
            vmComponent.next = mm.memoryComponents;
            mm.memoryComponents = &vmComponent;

            mm.dataStack = &vm;
            mm.pushStack = pushStackVM;
            mm.popStack = popStackVM;

            initNativeFunctionEnvironment(&vm);

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

            mm.memoryComponents = vmComponent.next;
            vmComponent.data = nullptr;
            vmComponent.markRoots = nullptr;
            vmComponent.handleWeakReferences = nullptr;
            vmComponent.next = nullptr;

            freeVM(&vm);
            freeMemoryManager(&mm);
        }
    }
}
