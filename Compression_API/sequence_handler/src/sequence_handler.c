#include "sequence_handler.h"
#include <stdio.h>

void CreateBlockSequence(const Sequence* seq, const char* outputFile) {
    FILE* file = fopen(outputFile, "ab");
    if (!file) return;

    fwrite(&seq->token, sizeof(uint8_t), 1, file);
    fwrite(seq->literals, sizeof(uint8_t), seq->literalLength, file);
    fwrite(&seq->matchOffset, sizeof(uint16_t), 1, file);
    fwrite(&seq->matchLength, sizeof(uint8_t), 1, file);

    fclose(file);
}

void PrintSequenceDetails(FILE* logFile, const Sequence* seq) {
    fprintf(logFile, "Token: %02X\n", seq->token);
    fprintf(logFile, "Match Offset: %u\n", seq->matchOffset);
    fprintf(logFile, "Match Length: %zu\n", seq->matchLength);
    fprintf(logFile, "Literal Length: %zu\n", seq->literalLength);
    fprintf(logFile, "Literals: ");
    for (size_t i = 0; i < seq->literalLength; ++i) {
        fprintf(logFile, "%c", seq->literals[i]);
    }
    fprintf(logFile, "\n");
}