#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "object.h"

ObjFunction* compile(Obj** objectRoot, const char* source);

#endif //CLOX_COMPILER_H
