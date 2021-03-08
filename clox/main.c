#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "file.h"

static void repl(VM* vm) {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(vm, line);
    }
}

static void runFile(VM* vm, const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(vm, source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
    VM vm;
    initVM(&vm);

    if (argc == 1) {
        repl(&vm);
    } else if (argc == 2) {
        runFile(&vm, argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    freeVM(&vm);
    return 0;
}
