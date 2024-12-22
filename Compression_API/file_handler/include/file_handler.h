#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stdint.h>
#include <stdio.h>

FILE* OpenFile(const char* fileName, const char* mode);
void CloseFile(FILE* file);

#endif