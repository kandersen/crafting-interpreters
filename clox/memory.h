#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

#define ALLOCATE(mm, type, count) \
    (type*)reallocate(mm, NULL, 0, sizeof(type) * (count))

#define FREE(mm, type, pointer) reallocate(mm, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(mm, type, pointer, oldCount, newCount) \
    (type*)reallocate(mm, pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(mm, type, pointer, oldCount) \
    reallocate(mm, pointer, sizeof(type) * (oldCount), 0)

typedef void (*MemoryComponentFn)(void*);
typedef void (*StackComponentPush)(void*, void*);
typedef void (*StackComponentPop)(void*);

typedef struct MemoryComponent {
    void* data;
    MemoryComponentFn markRoots;
    MemoryComponentFn handleWeakReferences;
    struct MemoryComponent* next;
} MemoryComponent;

typedef struct Obj Obj;

typedef struct MemoryManager {
    MemoryComponent* memoryComponents;
    Obj* objects;
    int grayCapacity;
    int grayCount;
    Obj** grayStack;

    void* dataStack;
    StackComponentPush pushStack;
    StackComponentPop popStack;

    size_t bytesAllocated;
    size_t nextGC;
} MemoryManager;

void initMemoryManager(MemoryManager* mm);
void freeMemoryManager(MemoryManager* mm);
void nullMemoryComponentFn(void* data);
void pushStack(MemoryManager* mm, void* data);
void popStack(MemoryManager* mm);

void collectGarbage(MemoryManager* mm);
void* reallocate(MemoryManager* mm, void* pointer, size_t oldSize, size_t newSize);


#endif //CLOX_MEMORY_H
