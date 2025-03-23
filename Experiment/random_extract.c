#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FILE_SIZE 10000000  // Adjust based on expected file size

void extract_random_passage(const char *filename, const char *output_file, int length) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    // Load entire file into memory
    char *buffer = malloc(MAX_FILE_SIZE);
    if (!buffer) {
        perror("Memory allocation error");
        fclose(file);
        return;
    }

    size_t file_size = fread(buffer, 1, MAX_FILE_SIZE, file);
    fclose(file);

    if (file_size == 0) {
        fprintf(stderr, "File is empty or read error occurred\n");
        free(buffer);
        return;
    }

    // Seed random number generator
    srand(time(NULL));

    // Choose a random starting point ensuring we stay within bounds
    int start_index = rand() % (file_size - length);
    
    // Extract passage and replace newlines with spaces
    char *passage = malloc(length + 1);
    if (!passage) {
        perror("Memory allocation error");
        free(buffer);
        return;
    }

    strncpy(passage, buffer + start_index, length);
    passage[length] = '\0';

    for (int i = 0; i < length; i++) {
        if (passage[i] == '\n' || passage[i] == '\r') {
            passage[i] = ' ';
        }
    }
    

    // Write output to a file
    FILE *out_file = fopen(output_file, "w");
    if (!out_file) {
        perror("Error opening output file");
        free(buffer);
        free(passage);
        return;
    }
    
    fprintf(out_file, "%s", passage);
    fclose(out_file);

    // Clean up
    free(buffer);
    free(passage);
}




