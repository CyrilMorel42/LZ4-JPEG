#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "random_image.c"

int runs = 10; 
int n = 12;

// Function to compute mean
double compute_mean(double times[], int size) {
    double sum = 0.0;
    double smallest =times[0];
    double largest = times[0];
    for (int i = 0; i < size; i++) {
        sum += times[i];
        if(smallest>times[i]){
            smallest = times[i];
        }
        if(largest<times[i]){
            largest = times[i];
        }
    }
    return (sum -largest-smallest) / (size-2);
}

double compute_median(double times[], int size) {
    if (size <= 2) {
        printf("[WARNING] Not enough data points to compute median after discarding extremes.\n");
        return 0.0;  // Return 0 or handle the case where there are too few elements
    }

    // Sort the execution times
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (times[i] > times[j]) {
                double temp = times[i];
                times[i] = times[j];
                times[j] = temp;
            }
        }
    }

    // Discard the smallest and largest values
    int new_size = size - 2;  
    double *trimmed_times = times + 1;  // Ignore first element (smallest)
    
    // Compute median for the remaining values
    if (new_size % 2 == 0) {
        return (trimmed_times[new_size / 2 - 1] + trimmed_times[new_size / 2]) / 2.0;
    } else {
        return trimmed_times[new_size / 2];
    }
}


int main() {
    ImageData image = {0};
    FILE *fp = fopen("results/JPEG_seq.exe_execution_times.json", "w");

    if (!fp) {
        printf("[ERROR] Failed to open execution_times.json for writing.\n");
        return 1;
    }

    fprintf(fp, "[\n");

    printf("Starting image processing pipeline...\n");

    for (size_t i = 0; i < n; i++) {
        int image_size = (int)pow(2, i);
        printf("[INFO] Processing image of size %dx%d\n", image_size, image_size);

        double execution_times[runs];

        fprintf(fp, "    {\n");
        fprintf(fp, "        \"image_size\": %d,\n", image_size);
        fprintf(fp, "        \"exe_name\": \"JPEG_seq.exe\",\n");
        fprintf(fp, "        \"execution_times_sec\": [\n");

        for (size_t j = 0; j < runs; j++) {
            printf("[INFO] Generating image - Run %lu\n", j + 1);

            image.height = image_size;
            image.width = image_size;
            image.pixel_count = image.height * image.width;

            generate_noise_image(&image);
            printf("[SUCCESS] Image generated: %d x %d\n", image.width, image.height);

            char buffer[256];
            int result = 1;
            double execution_time = 0.0;

            while (result != 0) {
                clock_t start_time = clock();

                // Open process and capture output
                FILE *pipe = popen("JPEG_seq.exe 2>&1", "r");
                if (!pipe) {
                    printf("[ERROR] Failed to execute JPEG_seq.exe\n");
                    return 1;
                }

                // Read and print error messages
                while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                    printf("[JPEG ERROR] %s", buffer);
                }

                result = pclose(pipe);
                clock_t end_time = clock();

                execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
                execution_times[j] = execution_time;

                free_pixels(image.pixels, image.height);
                printf("[MEMORY] Freed image memory for size %dx%d\n", image_size, image_size);

                if (result == 0) {
                    printf("[SUCCESS] JPEG processing successful for size %dx%d (Run %lu)\n", image_size, image_size, j + 1);
                } else {
                    printf("[ERROR] JPEG processing failed for size %dx%d (Run %lu) with exit code %d\n", image_size, image_size, j + 1, result);
                }
            }

            fprintf(fp, "            %.6f%s\n", execution_time, (j == runs - 1) ? "" : ",");
        }

        double mean = compute_mean(execution_times, runs);
        double median = compute_median(execution_times, runs);

        fprintf(fp, "        ],\n");
        fprintf(fp, "        \"mean_execution_time_sec\": %.6f,\n", mean);
        fprintf(fp, "        \"median_execution_time_sec\": %.6f\n", median);
        fprintf(fp, "    }%s\n", (i == n - 1) ? "" : ",");
    }

    fprintf(fp, "]\n");
    fclose(fp);
    printf("[SUCCESS] Execution times saved to execution_times.json\n");

    printf("Processing complete.\n");
    return 0;
}
