#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "object.h"
#include "vm.h"

ObjFunction* compile(Table* strings, Globals* globals, Obj** objectRoot, const char* source);

#endif //CLOX_COMPILER_H
