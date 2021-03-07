#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include <stdio.h>
#include "common.h"

typedef struct sObj Obj;
typedef struct sObjString ObjString;
typedef struct sObjFunction ObjFunction;
typedef struct sObjNative ObjNative;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
    VAL_UNDEFINED
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_UNDEFINED(value) ((value).type == VAL_UNDEFINED)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_OBJ(value)     ((value).as.obj)
#define AS_NUMBER(value)  ((value).as.number)

#define BOOL_VAL(value)   ((Value){ VAL_BOOL,      { .boolean = value }})
#define OBJ_VAL(object)   ((Value){ VAL_OBJ,       { .obj = (Obj*)object }})
#define NUMBER_VAL(value) ((Value){ VAL_NUMBER,    { .number = value }})
#define NIL_VAL           ((Value){ VAL_NIL,       { .number = 0 }})
#define UNDEFINED_VAL     ((Value){ VAL_UNDEFINED, { .number = 0 }})

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
bool valuesEqual(Value a, Value b);
void printValue(FILE* out, Value value);

#endif //CLOX_VALUE_H
