#include <stdlib.h>

#include "memory.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif


static void freeObject(MemoryManager* mm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(mm, &function->chunk);
            FREE(mm, ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(mm, ObjNative, object);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(mm, char, string->chars, string->length + 1);
            FREE(mm, ObjString, object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) object;
            FREE_ARRAY(mm, ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(mm, ObjClosure, object);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(mm, ObjUpvalue, object);
            break;
        }
    }
}

static void traceReferences(__unused MemoryManager* mm) {

}

static void sweep(__unused MemoryManager* mm) {

}

void* reallocate(MemoryManager* mm, void* pointer, size_t oldSize, size_t newSize) {
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(mm);
#endif
    }
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}


void freeMemoryManager(MemoryManager* mm) {
    Obj* object = mm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(mm, object);
        object = next;
    }
    initMemoryManager(mm);
}

void initMemoryManager(MemoryManager* memoryManager) {
    memoryManager->objects = NULL;
    memoryManager->memoryComponents = NULL;
}

void collectGarbage(MemoryManager* mm) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif
    for (
            MemoryComponent* currentComponent = mm->memoryComponents;
            currentComponent != NULL;
            currentComponent = currentComponent->next)
    {
        currentComponent->markRoots(currentComponent->data);
    }
    traceReferences(mm);

    for (
            MemoryComponent* currentComponent = mm->memoryComponents;
            currentComponent != NULL;
            currentComponent = currentComponent->next)
    {
        currentComponent->handleWeakReferences(currentComponent->data);
    }

    sweep(mm);

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
}

void nullMemoryComponentFn(__unused void* data) { }
