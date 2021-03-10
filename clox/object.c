#include <stdio.h>
#include <libc.h>
#include "object.h"
#include "memory.h"
#include "table.h"

#define ALLOCATE_OBJ(objectRoot, type, objectType) \
    (type*)allocateObject(objectRoot, sizeof(type), objectType)

static Obj* allocateObject(Obj** objectRoot, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = *objectRoot;
    *objectRoot = object;

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
    }
}

ObjFunction* newFunction(Obj** objectRoot) {
    ObjFunction* function = ALLOCATE_OBJ(objectRoot, ObjFunction, OBJ_FUNCTION);

    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative* newNative(Obj** objectRoot, int arity, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(objectRoot, ObjNative, OBJ_NATIVE);
    native->arity = arity;
    native->function = function;
    return native;
}

static ObjString* allocateString(Table* strings, Obj** objectRoot, char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(objectRoot, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tableSet(strings, string, NIL_VAL);

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

ObjString* copyString(Table* strings, Obj** objectRoot, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(strings, objectRoot, heapChars, length, hash);
}


ObjString* takeString(Table* strings, Obj** objectRoot, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(strings, objectRoot, chars, length, hash);
}
