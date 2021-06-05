#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "common.h"
#include "value.h"
#include "memory.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    MemoryManager* memoryManager;
    int count;
    int capacity;
    Entry* entries;
} Table;

//TODO(kjaa): initTable should take and store the allocator!
//            that would save every subsequent op from having to be passed an allocator.
void initTable(Table* table, MemoryManager* mm);
void freeTable(Table* table);

bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveUnmarked(Table* table);
void markTable(Table* table);

#endif //CLOX_TABLE_H
