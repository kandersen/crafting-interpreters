#include <stdio.h>
#include "object.h"
#include "memory.h"

#define ALLOCATE_OBJ(objectRoot, type, objectType) \
    (type*)allocateObject(objectRoot, sizeof(type), objectType)

static Obj* allocateObject(Obj** objectRoot, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = *objectRoot;
    *objectRoot = object;

    return object;
}


static void printFunction(ObjFunction* function) {
    if (function->name == NULL ) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE: {
            printf("<native fn>");
            break;
        }
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
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

