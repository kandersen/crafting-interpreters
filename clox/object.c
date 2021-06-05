#include <stdio.h>
#include <libc.h>
#include "object.h"
#include "memory.h"
#include "table.h"

#define ALLOCATE_OBJ(mm, type, objectType) \
    (type*)allocateObject(mm, sizeof(type), objectType)

static Obj* allocateObject(MemoryManager* mm, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(mm, NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = mm->objects;
    mm->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

static void printFunction(FILE* out, ObjFunction* function) {
    if (function->name == NULL ) {
        fprintf(out, "<script>");
        return;
    }
    fprintf(out, "<fn %s>", function->name->chars);
}

void printObject(FILE* out, Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: {
            printFunction(out, AS_BOUND_METHOD(value)->method->function);
            break;
        }
        case OBJ_CLASS: {
            fprintf(out, "%s", AS_CLASS(value)->name->chars);
            break;
        }
        case OBJ_FUNCTION:
            printFunction(out, AS_FUNCTION(value));
            break;
        case OBJ_NATIVE: {
            fprintf(out, "<native fn>");
            break;
        }
        case OBJ_STRING:
            fprintf(out, "%s", AS_CSTRING(value));
            break;
        case OBJ_CLOSURE:
            printFunction(out, AS_CLOSURE(value)->function);
            break;
        case OBJ_UPVALUE:
            fprintf(out, "upvalue");
            break;
        case OBJ_INSTANCE:
            fprintf(out, "%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
    }
}

ObjBoundMethod* newBoundMethod(MemoryManager* mm, Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(mm, ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* newClass(MemoryManager* mm, ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(mm, ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods, mm);
    return klass;
}

ObjClosure* newClosure(MemoryManager* mm, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(mm, ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure  = ALLOCATE_OBJ(mm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjUpvalue* newUpvalue(MemoryManager* mm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(mm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

ObjFunction* newFunction(MemoryManager* mm) {
    ObjFunction* function = ALLOCATE_OBJ(mm, ObjFunction, OBJ_FUNCTION);
    function->upvalueCount = 0;
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(MemoryManager* mm, ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(mm, ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields, mm);
    return instance;
}

ObjNative* newNative(MemoryManager* mm, int arity, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(mm, ObjNative, OBJ_NATIVE);
    native->arity = arity;
    native->function = function;
    return native;
}

static ObjString* allocateString(MemoryManager* mm, Table* strings, char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(mm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    Value stringForStack = OBJ_VAL(string);
    pushStack(mm, &stringForStack);
    tableSet(strings, string, NIL_VAL);
    popStack(mm);

    return string;
}

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= (uint32_t) key[i];
        hash *= 1677619u;
    }
    return hash;
}

ObjString* copyString(MemoryManager* mm, Table* strings, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(mm, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(mm, strings, heapChars, length, hash);
}


ObjString* takeString(MemoryManager* mm, Table* strings, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(mm, char, chars, length + 1);
        return interned;
    }

    return allocateString(mm, strings, chars, length, hash);
}
