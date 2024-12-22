#ifndef COMPRESSION_ENGINE_H
#define COMPRESSION_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include "sequence_handler.h"

uint8_t FindLongestMatch(uint8_t* input, size_t currentIndex, uint8_t* distance);
void LZ4Encode(uint8_t* input, size_t inputSize, const char* outputFile);

#endif