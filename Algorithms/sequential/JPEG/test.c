#include <stdio.h>
#include <stdlib.h>

void zigzag_pattern(size_t width, size_t height, double* input, double* output) {
    size_t index = 0;
    for (size_t n = 0; n < width + height - 1; n++) {
        size_t start = (n < height) ? 0 : n - height + 1;
        size_t end = (n < width) ? n : width - 1;

        if (n % 2 == 0) {
            for (size_t i = start; i <= end; i++) {
                size_t x = n - i;
                size_t y = i;
                output[index++] = input[x * width + y];
            }
        } else {
            for (size_t i = start; i <= end; i++) {
                size_t x = i;
                size_t y = n - i;
                output[index++] = input[x * width + y];
            }
        }
    }
}

void reverse_zigzag_pattern(size_t width, size_t height, double* input, double* output) {
    size_t index = 0;
    for (size_t n = 0; n < width + height - 1; n++) {
        size_t start = (n < height) ? 0 : n - height + 1;
        size_t end = (n < width) ? n : width - 1;

        if (n % 2 == 0) {
            for (size_t i = start; i <= end; i++) {
                size_t x = n - i;
                size_t y = i;
                output[x * width + y] = input[index++];
            }
        } else {
            for (size_t i = start; i <= end; i++) {
                size_t x = i;
                size_t y = n - i;
                output[x * width + y] = input[index++];
            }
        }
    }
}

int main() {
    double input[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    double* output = malloc(sizeof(double) * 16);

    zigzag_pattern(4, 4, input, output);
    printf("Zigzag Pattern:\n");
    for (size_t n = 0; n < 16; n++) {
        printf("%.0f ", output[n]);
        if ((n + 1) % 4 == 0) {
            printf("\n");
        }
    }

    printf("\n");

    double* reverse_output = malloc(sizeof(double) * 16);
    reverse_zigzag_pattern(4, 4, output, reverse_output);

    printf("Reverse Zigzag Pattern (Reconstructed Original):\n");
    for (size_t n = 0; n < 16; n++) {
        printf("%.0f ", reverse_output[n]);
        if ((n + 1) % 4 == 0) {
            printf("\n");
        }
    }

    free(output);
    free(reverse_output);

    return 0;
}
