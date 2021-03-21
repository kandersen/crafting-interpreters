#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include <stdio.h>
#include "chunk.h"
#include "table.h"
#include "compiler.h"

void disassembleChunk(FILE* out, Chunk* chunk, const char* name);
int disassembleInstruction(FILE* out, Chunk* chunk, int offset);

void dumpInternedStrings(FILE* out, Table* table);
void dumpGlobals(FILE* out, Globals* globals);
void dumpConstants(FILE* out, Chunk* chunk);

#endif //CLOX_DEBUG_H
