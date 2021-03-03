#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "vm.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING
} ObjType;

struct sObj {
    ObjType type;
    struct sObj* next;
};

struct sObjFunction {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
};

typedef Value (*NativeFn)(int argCount, Value* args);

struct sObjNative {
    Obj obj;
    NativeFn function;
};

struct sObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

ObjFunction* newFunction(VM* vm);

ObjString* takeString(VM* vm, char* chars, int length);
ObjString* copyString(VM* vm, const char* chars, int length);
ObjNative* newNative(VM* vm, NativeFn function);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif