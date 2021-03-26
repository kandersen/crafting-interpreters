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
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(MemoryManager* mm, Table* table);

bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(MemoryManager* mm, Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(MemoryManager* mm, Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void markTable(Table* table);

#endif //CLOX_TABLE_H
