#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "value.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    Table names;
    uint8_t count;
    Value values[UINT8_COUNT];
    ValueArray states;
} Globals;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    Table strings;
    Globals globals;
    Obj* objects;

    FILE* outPipe;
    FILE* errPipe;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initGlobals(Globals* globals);
void freeGlobals(Globals* globals);

void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, const char* source);

#endif //CLOX_VM_H
