#ifndef SEQUENCE_HANDLER_H
#define SEQUENCE_HANDLER_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

typedef struct {
    uint8_t token;
    uint8_t* literals;
    size_t literalLength;
    uint16_t matchOffset;
    size_t matchLength;
} Sequence;

// Public API for sequences
void CreateBlockSequence(const Sequence* seq, const char* outputFile);
void PrintSequenceDetails(FILE* logFile, const Sequence* seq);

#endif