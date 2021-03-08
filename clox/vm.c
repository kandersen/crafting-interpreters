#include <string.h>
#include <stdarg.h>

#include "vm.h"
#include "memory.h"
#include "compiler.h"
#include "debug.h"

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
}

static void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

static Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM* vm) {
    ObjString* b = AS_STRING(pop(vm));
    ObjString* a = AS_STRING(pop(vm));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(&vm->strings, &vm->objects, chars, length);
    push(vm, OBJ_VAL(result));
}

static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        ObjFunction* function = frame->function;

        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()", function->name->chars);
        }
    }

    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    size_t instruction = (size_t) (frame->ip - frame->function->chunk.code - 1);
    int line = frame->function->chunk.lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);

    resetStack(vm);
}


static bool call(VM* vm, ObjFunction* function, int argCount) {
    if (argCount != function->arity) {
        runtimeError(vm, "Expected %d arguments but got %d.", function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;

    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

static bool callValue(VM* vm, Value callee, int argCount) {
    if (IS_OBJ(callee))  {
        switch (OBJ_TYPE(callee)) {
            case OBJ_FUNCTION:
                return call(vm, AS_FUNCTION(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm->stackTop - argCount);
                vm->stackTop -= argCount + 1;
                push(vm, result);
                return true;
            }
            default:
                break;
        }
    }

    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
            runtimeError(vm, "Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, valueType(a op b)); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(stdout, *slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(stdout, &frame->function->chunk, (int) (frame->ip - frame->function->chunk.code));
#endif

        switch (READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_NIL: {
                push(vm, NIL_VAL);
                break;
            }
            case OP_TRUE: {
                push(vm, BOOL_VAL(true));
                break;
            }
            case OP_FALSE: {
                push(vm, BOOL_VAL(false));
                break;
            }
            case OP_POP: {
                pop(vm);
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_GET_GLOBAL: {
                Value value = vm->globals.values[READ_BYTE()];
                if (IS_UNDEFINED(value)) {
                    runtimeError(vm, "Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                vm->globals.values[READ_BYTE()] = pop(vm);
                break;
            }
            case OP_SET_GLOBAL: {
                uint8_t index = READ_BYTE();
                if (IS_UNDEFINED(vm->globals.values[index])) {
                    runtimeError(vm, "Undefined variable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->globals.values[index] = peek(vm, 0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate(vm);
                } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtimeError(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_NOT: {
                push(vm, BOOL_VAL(isFalsey(pop(vm))));
                break;
            }
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtimeError(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            }

            case OP_PRINT: {
                printValue(vm->outPipe, pop(vm));
                fprintf(vm->outPipe, "\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(vm, peek(vm, argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RETURN: {
                Value result = pop(vm);

                vm->frameCount--;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                vm->stackTop = frame->slots;
                push(vm, result);

                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
#undef BINARY_OP
}

void initGlobals(Globals* globals) {
    initTable(&globals->names);
    globals->count = 0;
    initValueArray(&globals->states);
}

static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void defineNative(VM* vm, const char* name, NativeFn function) {
    push(vm, OBJ_VAL((copyString(vm, name, (int)strlen(name)))));
    push(vm, OBJ_VAL(newNative(vm, function)));
    tableSet(&vm->globalNames, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
}

void initVM(VM* vm) {
    resetStack(vm);
    vm->objects = NULL;
    initTable(&vm->strings);
    initGlobals(&vm->globals);
    vm->outPipe = stdout;
    vm->errPipe = stderr;
//    defineNative(vm, "clock", clockNative);
}

void freeGlobals(Globals* globals) {
    freeTable(&globals->names);
    freeValueArray(&globals->states);
}

void freeVM(VM* vm) {
    freeObjects(vm->objects);
    freeTable(&vm->strings);
    freeGlobals(&vm->globals);
    initVM(vm);
}



InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(&vm->strings, &vm->globals, &vm->objects, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(vm, OBJ_VAL(function));
    callValue(vm, OBJ_VAL(function), 0);

    return run(vm);
}
