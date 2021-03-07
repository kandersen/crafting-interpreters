#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include <stdio.h>
#include "chunk.h"

void disassembleChunk(FILE* out, Chunk* chunk, const char* name);
int disassembleInstruction(FILE* out, Chunk* chunk, int offset);

#endif //CLOX_DEBUG_H
