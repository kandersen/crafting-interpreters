#include "chunk.h"
#include "value.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void writeChunk(MemoryManager* mm, Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(mm, uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(mm, int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void freeChunk(MemoryManager* mm, Chunk* chunk) {
    FREE_ARRAY(mm, uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(mm, int, chunk->lines, chunk->capacity);
    freeValueArray(mm, &chunk->constants);
    initChunk(chunk);
}

int addConstant(MemoryManager* mm, Chunk* chunk, Value value) {
    writeValueArray(mm, &chunk->constants, value);
    return chunk->constants.count - 1;
}
