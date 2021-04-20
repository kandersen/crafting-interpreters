#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "vm.h"
#include "memory.h"
#include "compiler.h"
#include "debug.h"

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
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
    ObjString* b = AS_STRING(peek(vm, 0));
    ObjString* a = AS_STRING(peek(vm, 1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(vm->mm, char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(vm->mm, &vm->strings, chars, length);
    pop(vm);
    pop(vm);
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
        ObjFunction* function = frame->closure->function;

        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack(vm);
}


static bool call(VM* vm, ObjClosure * closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(vm, "Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;

    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

static bool nativeCall(VM* vm, ObjNative* nativeObj, int argCount) {
    if (argCount != nativeObj->arity) {
        runtimeError(vm, "Expected %d arguments but got %d.", nativeObj->arity, argCount);
        return false;
    }

    NativeFn native = nativeObj->function;
    Value result;
    if(!native(argCount, vm->stackTop - argCount, &result)) {
        runtimeError(vm, "Error in native call.");
    }
    vm->stackTop -= argCount + 1;
    push(vm, result);
    return true;
}

static bool callValue(VM* vm, Value callee, int argCount) {
    if (IS_OBJ(callee))  {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLASS: {
                ObjClass *klass = AS_CLASS(callee);
                vm->stackTop[-argCount - 1] = OBJ_VAL(newInstance(vm->mm, klass));
                return true;
            }
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                return nativeCall(vm, AS_NATIVE(callee), argCount);
            }
            default:
                break;
        }
    }

    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm->openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(vm->mm, local);
    createdUpvalue->next = upvalue;
    // TODO(kjaa): you can use pointer-to-a-pointer to avoid this special casing:
    //   prevUpvalue = &vm->openUpvalue; while() { prevUpvalue = &upvalue->next };
    //   *prevUpvalue = createdUpvalue;
    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
        disassembleInstruction(stdout, &frame->closure->function->chunk, (int) (frame->ip - frame->closure->function->chunk.code));
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
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(vm, *frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(vm, 0);
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
            case OP_CLOSURE: {
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = newClosure(vm->mm, function);
                push(vm, OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        ObjUpvalue* capturedUpvalue = frame->closure->upvalues[index];
                        closure->upvalues[i] = capturedUpvalue;
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
            }
            case OP_RETURN: {
                Value result = pop(vm);

                closeUpvalues(vm, frame->slots);

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
            case OP_CLASS: {
                push(vm, OBJ_VAL(newClass(vm->mm, READ_STRING())));
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(vm, 0))) {
                    runtimeError(vm, "Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(vm, 0));
                ObjString* name = READ_STRING();

                Value value;
                if (tableGet(&instance->fields, name, &value)) {
                    pop(vm); // Instance.
                    push(vm, value);
                    break;
                } else {
                    runtimeError(vm, "Undefined property '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(vm, 1))) {
                    runtimeError(vm, "Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjInstance* instance = AS_INSTANCE(peek(vm, 1));
                tableSet(vm->mm, &instance->fields, READ_STRING(), peek(vm, 0));
                Value value = pop(vm);
                pop(vm);
                push(vm, value);
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
    initTable(&globals->names);\
    globals->count = 0;
}

static int clockNative(__unused int argCount, __unused Value* args, Value* result) {
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return true;
}

static void defineNative(VM* vm, const char* name, int arity, NativeFn function) {
    ObjString* identifier = copyString(vm->mm, &vm->strings, name, (int) strlen(name));
    push(vm, OBJ_VAL(identifier));
    push(vm, OBJ_VAL(newNative(vm->mm, arity, function)));

    uint8_t newIndex = (uint8_t)vm->globals.count++;
    vm->globals.values[newIndex] = vm->stack[1];
    vm->globals.identifiers[newIndex] = identifier;
    tableSet(vm->mm, &vm->globals.names, AS_STRING(vm->stack[0]), NUMBER_VAL((double)newIndex));

    pop(vm);
    pop(vm);
}

void handleWeakVMReferences(void* data) {
    VM* vm = (VM*)data;
    tableRemoveUnmarked(&vm->strings);
}

void markVMRoots(void* data) {
    VM* vm = (VM*)data;
    // The Stack
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm->mm, *slot);
    }

    // CallFrames
    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm->mm, (Obj*)vm->frames[i].closure);
    }

    // Open Upvalues
    for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(vm->mm, (Obj*)upvalue);
    }

    // Global variables and associated data
    for (int i = 0; i < vm->globals.count; i++) {
        markValue(vm->mm, vm->globals.values[i]);
        markObject(vm->mm, (Obj*)vm->globals.identifiers[i]);
    }
    markTable(vm->mm, &vm->globals.names);
}

void initVM(VM* vm) {
    resetStack(vm);
    initTable(&vm->strings);
    initGlobals(&vm->globals);

    vm->outPipe = stdout;
    vm->errPipe = stderr;

    vm->mm = NULL;
}

void initNativeFunctionEnvironment(VM* vm) {
    defineNative(vm, "clock", 0, clockNative);
}

void freeGlobals(MemoryManager* mm, Globals* globals) {
    freeTable(mm, &globals->names);
}

void freeVM(VM* vm) {
    freeTable(vm->mm, &vm->strings);
    freeGlobals(vm->mm, &vm->globals);
    initVM(vm);
}

InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(vm->mm, &vm->strings, &vm->globals, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(vm, OBJ_VAL(function));
    ObjClosure* closure = newClosure(vm->mm, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    callValue(vm, OBJ_VAL(closure), 0);

    return run(vm);
}

//typedef void (*StackComponentPush)(void*, void*);
//typedef void (*StackComponentPop)(void*);

void pushStackVM(void* data, void* value) {
    push((VM*)data, *((Value*)value));
}

void popStackVM(void* data) {
    pop((VM*)data);
}

