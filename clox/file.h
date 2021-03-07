#ifndef CLOX_FILE_H
#define CLOX_FILE_H

#include <stdio.h>

char* readFileHandle(FILE* file, const char* name);
char* readFile(const char* path);

#endif //CLOX_FILE_H
