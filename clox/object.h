#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value)))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
    int upvalueCount;
} ObjFunction;

typedef int (*NativeFn)(int argCount, Value* args, Value* result);

typedef struct {
    Obj obj;
    int arity;
    NativeFn function;
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    struct ObjUpvalue* next;
    Value closed;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjClosure* newClosure(Obj** objectRoot, ObjFunction* function);
ObjUpvalue* newUpvalue(Obj** objectRoot, Value* slot);
ObjFunction* newFunction(Obj** objectRoot);
ObjNative* newNative(Obj** objectRoot, int arity, NativeFn function);

ObjString* copyString(Table* strings, Obj** objectRoot, const char* chars, int length);
ObjString* takeString(Table* strings, Obj** objectRoot, char* chars, int length);

void printObject(FILE* out, Value value);

#endif //CLOX_OBJECT_H
