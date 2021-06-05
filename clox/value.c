#include <stdio.h>
#include <stdlib.h>

#include "value.h"
#include "memory.h"
#include "object.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(MemoryManager* mm, ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(mm, Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(MemoryManager* mm, ValueArray* array) {
    FREE_ARRAY(mm, Value, array->values, array->capacity);
    initValueArray(array);
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
    return a == b;
#else
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL:       return true;
        case VAL_BOOL:      return AS_BOOL(a)   == AS_BOOL(b);
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:       return AS_OBJ(a)    == AS_OBJ(b);
        case VAL_UNDEFINED:
            //TODO(kjaa): decide what UNDEF == UNDEF is.
            return false;
    }
#endif
}

void printValue(FILE* out, Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        fprintf(out, AS_BOOL(value) ? "true" : "false");
    } else if (IS_NIL(value)) {
        fprintf(out, "nil");
    } else if (IS_NUMBER(value)) {
        fprintf(out, "%g", AS_NUMBER(value));
    } else if (IS_OBJ(value)) {
        printObject(out, value);
    }
#else
    switch(value.type) {
        case VAL_NUMBER: fprintf(out, "%g", AS_NUMBER(value)); break;
        case VAL_BOOL: fprintf(out, AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: fprintf(out, "nil"); break;
        case VAL_OBJ: printObject(out, value); break;
        case VAL_UNDEFINED: fprintf(out, "<undefined>"); break;
    }
#endif
}


void markObject(MemoryManager* mm, Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(stdout, OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;

    if (mm->grayCapacity < mm->grayCount + 1) {
        mm->grayCapacity = GROW_CAPACITY(mm->grayCapacity);
        mm->grayStack = (Obj**)realloc(mm->grayStack, sizeof(Obj*) * mm->grayCapacity);
        if (mm->grayStack == NULL) exit(1);
    }
    mm->grayStack[mm->grayCount++] = object;
}

void markValue(MemoryManager* mm, Value value) {
    if (!IS_OBJ(value)) return;
    markObject(mm, AS_OBJ(value));
}

