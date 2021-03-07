#include <stdlib.h>
#include "file.h"

char *readFile(const char *path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    char* buffer = readFileHandle(file, path);

    fclose(file);
    return buffer;
}

char *readFileHandle(FILE *file, const char *name) {
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", name);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", name);
        exit(74);
    }

    buffer[bytesRead] = '\0';
    return buffer;
}
