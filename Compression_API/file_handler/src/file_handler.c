#include "file_handler.h"

FILE* OpenFile(const char* fileName, const char* mode) {
    return fopen(fileName, mode);
}

void CloseFile(FILE* file) {
    if (file) fclose(file);
}