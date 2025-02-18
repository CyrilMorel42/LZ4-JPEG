#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// Define a structure for ImageData
typedef struct {
    uint8_t r, g, b;  // RGB components
} Pixel;

typedef struct {
    size_t width, height;
    Pixel **pixels;
} ImageData;

// Function to clamp values within 0-255 range
uint8_t clamp(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

// Function to build the rChrominance matrix
void build_rChrominance_matrix(ImageData data, uint8_t*** matrix) {
    printf("Building rChrominance matrix...\n");
    *matrix = malloc(data.height * sizeof(uint8_t*));

    for (int y = 0; y < data.height; y++) {
        (*matrix)[y] = malloc(data.width * sizeof(uint8_t));
    }

    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            // Standard RGB to Cr (rChrominance)
            int CrValue = (int)(0.5 * data.pixels[y][x].r - 0.418688 * data.pixels[y][x].g - 0.081312 * data.pixels[y][x].b);
            printf("rChrominance (Cr) for pixel (%zu, %zu): %d\n", y, x, CrValue);  // Debug log for Cr
            (*matrix)[y][x] = clamp(CrValue);
        }
    }
}

// Function to build the bChrominance matrix
void build_bChrominance_matrix(ImageData data, uint8_t*** matrix) {
    printf("Building bChrominance matrix...\n");
    *matrix = malloc(data.height * sizeof(uint8_t*));

    for (int y = 0; y < data.height; y++) {
        (*matrix)[y] = malloc(data.width * sizeof(uint8_t));
    }

    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            // Standard RGB to Cb (bChrominance)
            int CbValue = (int)(-0.168736 * data.pixels[y][x].r - 0.331264 * data.pixels[y][x].g + 0.5 * data.pixels[y][x].b);
            printf("bChrominance (Cb) for pixel (%zu, %zu): %d\n", y, x, CbValue);  // Debug log for Cb
            (*matrix)[y][x] = clamp(CbValue);
        }
    }
}

// Function to free the dynamically allocated memory for matrices
void free_matrix(uint8_t** matrix, size_t height) {
    for (int i = 0; i < height; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

int main() {
    // Test data: a small 2x2 image with some basic colors
    ImageData data;
    data.width = 2;
    data.height = 2;

    // Allocate memory for pixel data
    data.pixels = malloc(data.height * sizeof(Pixel*));
    for (int i = 0; i < data.height; i++) {
        data.pixels[i] = malloc(data.width * sizeof(Pixel));
    }

    // Set test RGB values
    data.pixels[0][0] = (Pixel){255, 0, 0}; // Red
    data.pixels[0][1] = (Pixel){0, 255, 0}; // Green
    data.pixels[1][0] = (Pixel){0, 0, 255}; // Blue
    data.pixels[1][1] = (Pixel){255, 255, 0}; // Yellow

    // Initialize matrices for chrominance components
    uint8_t **rChrominanceMatrix = NULL;
    uint8_t **bChrominanceMatrix = NULL;

    // Call the functions to build the chrominance matrices
    build_rChrominance_matrix(data, &rChrominanceMatrix);
    build_bChrominance_matrix(data, &bChrominanceMatrix);

    // Print the results for rChrominance (Cr) and bChrominance (Cb)
    printf("rChrominance (Cr) Matrix:\n");
    for (int y = 0; y < data.height; y++) {
        for (int x = 0; x < data.width; x++) {
            printf("%d ", rChrominanceMatrix[y][x]);
        }
        printf("\n");
    }

    printf("\nbChrominance (Cb) Matrix:\n");
    for (int y = 0; y < data.height; y++) {
        for (int x = 0; x < data.width; x++) {
            printf("%d ", bChrominanceMatrix[y][x]);
        }
        printf("\n");
    }

    // Free dynamically allocated memory
    free_matrix(rChrominanceMatrix, data.height);
    free_matrix(bChrominanceMatrix, data.height);

    // Free pixel data
    for (int i = 0; i < data.height; i++) {
        free(data.pixels[i]);
    }
    free(data.pixels);

    return 0;
}
