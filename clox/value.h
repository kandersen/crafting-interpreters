#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include <stdio.h>

#include "common.h"

#ifdef NAN_BOXING
#include <string.h>

#define QNAN ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT ((uint64_t)0x8000000000000000)

#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

typedef uint64_t Value;

#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_NIL(value)    ((value) == NIL_VAL)
#define IS_BOOL(value)   (((value) | 1) == TRUE_VAL)
#define IS_OBJ(value)    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_NUMBER(value) valueToNum(value)
#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_OBJ(value)    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num) numToValue(num)
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define OBJ_VAL(obj)    (Value) (SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}


typedef struct Obj Obj;
typedef struct ObjString ObjString;
#else

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
    VAL_UNDEFINED
} ValueType;


typedef struct Value {
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

#endif

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

typedef struct MemoryManager MemoryManager;

void initValueArray(ValueArray* array);
void writeValueArray(MemoryManager* mm, ValueArray* array, Value value);
void freeValueArray(MemoryManager* mm, ValueArray* array);
bool valuesEqual(Value a, Value b);
void printValue(FILE* out, Value value);

void markObject(MemoryManager* mm, Obj* object);
void markValue(MemoryManager* mm, Value value);


#endif //CLOX_VALUE_H
