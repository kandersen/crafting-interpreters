#include <stdlib.h>

#include "memory.h"
#include "debug.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2


static void freeObject(MemoryManager* mm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            FREE(mm, ObjBoundMethod, object);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(&klass->methods);
            FREE(mm, ObjClass, object);
            break;
        }
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
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(&instance->fields);
            FREE(mm, ObjInstance, object);
            break;
        }
    }
}

static void markArray(MemoryManager* mm, ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(mm, array->values[i]);
    }
}

static void blackenObject(MemoryManager* mm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void *) object);
    printValue(stdout, OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(mm, bound->receiver);
            markObject(mm, (Obj*)bound->method);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject(mm, (Obj*)klass->name);
            markTable(&klass->methods);
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(mm, (Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(mm, (Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue *upvalue = (ObjUpvalue *) object;
            markValue(mm, upvalue->closed);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            markObject(mm, (Obj*)function->name);
            markArray(mm, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject(mm, (Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }
    }
}

static void traceReferences(MemoryManager* mm) {
    while (mm->grayCount > 0) {
        Obj* object = mm->grayStack[--mm->grayCount];
        blackenObject(mm, object);
    }
}

static void sweep(MemoryManager* mm) {
    Obj* previous = NULL;
    Obj* object = mm->objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                mm->objects = object;
            }

            freeObject(mm, unreached);
        }
    }
}

void* reallocate(MemoryManager* mm, void* pointer, size_t oldSize, size_t newSize) {
    mm->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(mm);
#endif
        if (mm->bytesAllocated > mm->nextGC) {
            collectGarbage(mm);
        }
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
    free(mm->grayStack);

    initMemoryManager(mm);
}

void initMemoryManager(MemoryManager* memoryManager) {
    memoryManager->objects = NULL;
    memoryManager->memoryComponents = NULL;

    memoryManager->grayCapacity = 0;
    memoryManager->grayCount = 0;
    memoryManager->grayStack = NULL;

    memoryManager->dataStack = NULL;
    memoryManager->pushStack = NULL;
    memoryManager->popStack = NULL;

    memoryManager->bytesAllocated = 0;
    memoryManager->nextGC = 1024 * 1024;
}

void collectGarbage(MemoryManager* mm) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = mm->bytesAllocated;
#endif
    // Mark all roots.
    for (
            MemoryComponent* currentComponent = mm->memoryComponents;
            currentComponent != NULL;
            currentComponent = currentComponent->next)
    {
        currentComponent->markRoots(currentComponent->data);
    }

    // Mark all objects transitively reachable from roots.
    traceReferences(mm);

    // Account for weak references.
    for (
            MemoryComponent* currentComponent = mm->memoryComponents;
            currentComponent != NULL;
            currentComponent = currentComponent->next)
    {
        currentComponent->handleWeakReferences(currentComponent->data);
    }

    // Clear unmarked objects.
    sweep(mm);

    mm->nextGC = mm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");;
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - mm->bytesAllocated, before, mm->bytesAllocated, mm->nextGC);
#endif
}

void nullMemoryComponentFn(__unused void* data) { }

void pushStack(MemoryManager *mm, void *data) {
    mm->pushStack(mm->dataStack, data);
}

void popStack(MemoryManager *mm) {
    mm->popStack(mm->dataStack);
}
