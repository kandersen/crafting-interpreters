#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "object.h"

static int simpleInstruction(FILE* out, const char* name, int offset) {
    fprintf(out, "%s\n", name);
    return offset + 1;
}

static int constantInstruction(FILE* out, const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    fprintf(out, "%-16s %4d '", name, constant);
    printValue(out, chunk->constants.values[constant]);
    fprintf(out, "'\n");
    return offset + 2;
}

static int byteInstruction(FILE* out, const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    fprintf(out, "%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(FILE* out, const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    fprintf(out, "%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

void disassembleChunk(FILE* out, Chunk* chunk, const char* name) {
    fprintf(out, "== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(out, chunk, offset);
    }
}

int disassembleInstruction(FILE* out, Chunk* chunk, int offset ){

    fprintf(out, "%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        fprintf(out, "   | ");
    } else {
        fprintf(out, "%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction(out, "OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction(out, "OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction(out, "OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction(out, "OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction(out, "OP_POP", offset);
        case OP_GET_LOCAL:
            return byteInstruction(out, "OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction(out, "OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return byteInstruction(out, "OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return byteInstruction(out, "OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return byteInstruction(out, "OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE:
            return byteInstruction(out, "OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byteInstruction(out, "OP_SET_UPVALUE", chunk, offset);
        case OP_EQUAL:
            return simpleInstruction(out, "OP_EQUAL", offset);
        case OP_LESS:
            return simpleInstruction(out, "OP_LESS", offset);
        case OP_GREATER:
            return simpleInstruction(out, "OP_GREATER", offset);
        case OP_ADD:
            return simpleInstruction(out, "OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction(out, "OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction(out, "OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction(out, "OP_DIVIDE", offset);
        case OP_NEGATE:
            return simpleInstruction(out, "OP_NEGATE", offset);
        case OP_PRINT:
            return simpleInstruction(out, "OP_PRINT", offset);
        case OP_NOT:
            return simpleInstruction(out, "OP_NOT", offset);
        case OP_JUMP:
            return jumpInstruction(out, "OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction(out, "OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction(out, "OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byteInstruction(out, "OP_CALL", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            fprintf(out, "%-16s %4d ", "OP_CLOSURE", constant);
            printValue(out, chunk->constants.values[constant]);
            fprintf(out, "\n");

            ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                fprintf(out, "%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
            }
            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return simpleInstruction(out, "OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:
            return simpleInstruction(out, "OP_RETURN", offset);
        case OP_CLASS: {
            uint8_t constant = chunk->code[offset + 1];
            fprintf(out, "%-16s %4d '", "OP_CLASS", constant);
            printValue(out, chunk->constants.values[constant]);
            fprintf(out, "'\n");
            return offset + 2;
        }
        case OP_SET_PROPERTY: {
            return constantInstruction(out, "OP_SET_PROPERTY", chunk, offset);
        }
        case OP_GET_PROPERTY: {
            return constantInstruction(out, "OP_GET_PROPERTY", chunk, offset);
        }
        default:
            fprintf(out, "Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void dumpInternedStrings(FILE* out, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        if (table->entries[i].key != NULL) {
            printValue(out, OBJ_VAL(table->entries[i].key));
            fprintf(out,"\n");
        }
    }
}

void dumpGlobals(FILE* out, Globals* globals) {
    for (int i = 0; i < globals->count; i++) {
        ObjString* identifier = globals->identifiers[i];
        printValue(out, OBJ_VAL(identifier));
        fprintf(out, "\n");
    }
}

void dumpConstants(FILE* out, Chunk* chunk) {
    for (int i = 0; i < chunk->constants.count; i++) {
        fprintf(out, "%d:  ", i);
        printValue(out, chunk->constants.values[i]);
        fprintf(out,"\n");
    }
}
