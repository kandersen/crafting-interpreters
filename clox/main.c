#include <stdio.h>
#include "common.h"

#include "vm.h"

int main() {
    VM vm;
    initVM(&vm);
    printf("Hello, %d!\n", interpret(&vm, "print(3+4);"));
    freeVM(&vm);
    return 0;
}
