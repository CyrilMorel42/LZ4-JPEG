#include "compression_engine.h"
#include "file_handler.h"
#include <stdio.h>

#define MAX_MATCH_LENGTH 255
#define MIN_MATCH_LENGTH 4
#define WINDOW_SIZE 65536

uint8_t FindLongestMatch(uint8_t* input, size_t currentIndex, uint8_t* distance) {
    size_t matchLength = 0;
    size_t matchDistance = 0;
    size_t windowStart = (currentIndex >= WINDOW_SIZE) ? currentIndex - WINDOW_SIZE : 0;

    for (size_t i = windowStart; i < currentIndex; ++i) {
        size_t currentMatchLength = 0;
        while (currentMatchLength < MAX_MATCH_LENGTH && input[i + currentMatchLength] == input[currentIndex + currentMatchLength]) {
            ++currentMatchLength;
        }
        if (currentMatchLength > matchLength) {
            matchLength = currentMatchLength;
            matchDistance = currentIndex - i;
        }
    }

    if (matchLength >= MIN_MATCH_LENGTH) {
        *distance = (uint8_t)matchDistance;
        return (uint8_t)matchLength;
    }
    return 0;
}

void LZ4Encode(uint8_t* input, size_t inputSize, const char* outputFile) {
    Sequence currentSequence;
    size_t inputIndex = 0;
    uint16_t litCounter = 0;

    while (inputIndex < inputSize) {
        uint8_t matchDistance = 0;
        uint8_t matchLength = FindLongestMatch(input, inputIndex, &matchDistance);

        if (matchLength < MIN_MATCH_LENGTH) {
            if (litCounter == 0) {
                currentSequence.literals = &input[inputIndex];
            }
            ++inputIndex;
            ++litCounter;
        } else {
            currentSequence.matchOffset = matchDistance;
            currentSequence.literalLength = litCounter;
            currentSequence.matchLength = matchLength;
            CreateBlockSequence(&currentSequence, outputFile); // From Sequence module
            litCounter = 0;
            inputIndex += matchLength;
        }
    }

    if (litCounter > 0) {
        currentSequence.matchOffset = 0;
        currentSequence.literalLength = litCounter;
        currentSequence.matchLength = 0;
        CreateBlockSequence(&currentSequence, outputFile);
    }
}
