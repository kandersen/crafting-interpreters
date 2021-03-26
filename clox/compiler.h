#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "object.h"
#include "vm.h"

ObjFunction* compile(MemoryManager* mm, Table* strings, Globals* globals, const char* source);

#endif //CLOX_COMPILER_H
