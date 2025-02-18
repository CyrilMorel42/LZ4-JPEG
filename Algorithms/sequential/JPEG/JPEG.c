#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include "stb_image.h"
#include "stb_image_write.h"



#define IMAGES_DIRECTORY "../../../Assets/Images/"
#define OUTPUT_DIRECTORY "../../../Output-Input/Images/"
#define PI 3.14159265358979323846
size_t LUMINANCE_QUANTIZATION_TABLE[64] = {
    8,   6,   6,   8,   10,  14,  18,  22,
    6,   6,   7,   9,   12,  20,  22,  20,
    6,   7,   8,   10,  14,  22,  25,  22,
    8,   9,   10,  14,  18,  28,  27,  22,
    10,  12,  14,  18,  22,  35,  33,  26,
    14,  18,  22,  22,  27,  33,  36,  30,
    18,  22,  26,  28,  33,  40,  40,  34,
    22,  26,  28,  30,  36,  34,  35,  33
};




size_t CHROMINANCE_QUANTIZATION_TABLE [32] = {
17,  18,  24,  47  ,
18,  21,  26,  66,  
24,  26,  56,  99,  
47,  66,  99,  99,  
66,  99,  99,  99,  
99,  99,  99 , 99,  
99,  99 , 99,  99,  
99,  99 , 99,  99,  

    };


typedef struct {
    unsigned char r, g, b, a;
} Pixel;

typedef struct {
    Pixel **pixels;
    int height;
    int width;
    size_t pixel_count;
} ImageData;

typedef struct {
    uint8_t lum_values[64];
    uint8_t b_values[32];
    uint8_t r_values[32];

    size_t index;
    double* lum_coefficients;
    double* r_coefficients;
    double* b_coefficients;
    int* RLE_encoded_lum;
    int* RLE_encoded_r;
    int* RLE_encoded_b;
} PixelGroup;

void free_pixels(Pixel** pixels, int height) {
    for (int y = 0; y < height; y++) {
        free(pixels[y]);
    }
    free(pixels);
}

ImageData read_image(const char *filename) {
    int width, height, channels;

    unsigned char *data = stbi_load(filename, &width, &height, &channels, 0);
    if (data == NULL) {
        printf("Error loading image\n");
        exit(1);
    }

    Pixel **pixels = malloc(height * sizeof(Pixel *));
    for (int y = 0; y < height; y++) {
        pixels[y] = malloc(width * sizeof(Pixel));
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_index = (y * width + x) * channels;
            pixels[y][x].r = data[pixel_index];                  
            pixels[y][x].g = data[pixel_index + 1];              
            pixels[y][x].b = data[pixel_index + 2];              
            pixels[y][x].a = (channels == 4) ? data[pixel_index + 3] : 255;  
        }
    }

    ImageData result;
    result.pixels = pixels;
    result.height = height;
    result.width = width;
    result.pixel_count = height * width;

    stbi_image_free(data);
    return result;
}

char* construct_path(char* image_name, char* directory) {
    char* path = malloc(strlen(image_name) + strlen(directory) + 1);
    strcpy(path, directory);
    strcat(path, image_name);
    return path;
}

void build_luminance_matrix(ImageData data, uint8_t*** matrix) {
    printf("Building luminance matrix...\n");
    *matrix = malloc(data.height * sizeof(uint8_t*));
    for (int y = 0; y < data.height; y++) {
        (*matrix)[y] = malloc(data.width * sizeof(uint8_t));
    }

    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            (*matrix)[y][x] = 0.299 * data.pixels[y][x].r + 0.587 * data.pixels[y][x].g + 0.114 * data.pixels[y][x].b;
        }
    }
}

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
            int CrValue = (int)(0.439 * data.pixels[y][x].r - 0.368 * data.pixels[y][x].g - 0.071 * data.pixels[y][x].b + 128);
            //printf("rChrominance (Cr) for pixel (%zu, %zu): %d\n", y, x, CrValue);  // Debug log for Cr
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
            int CbValue = (int)(-0.148 * data.pixels[y][x].r - 0.291 * data.pixels[y][x].g + 0.439 * data.pixels[y][x].b+128);
           // printf("bChrominance (Cb) for pixel (%zu, %zu): %d\n", y, x, CbValue);  // Debug log for Cb
            (*matrix)[y][x] = clamp(CbValue);
        }
    }
}

void create_png_image(char* filename, int width, int height, Pixel** pixels) {
    unsigned char* rgb_image = malloc(width * height * 4); 

    if (rgb_image == NULL) {
        printf("Error: Unable to allocate memory for the image.\n");
        return;
    }


    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t index = (y * width + x) * 4;  
            rgb_image[index + 0] = pixels[y][x].r;  
            rgb_image[index + 1] = pixels[y][x].g;  
            rgb_image[index + 2] = pixels[y][x].b;  
            rgb_image[index + 3] = pixels[y][x].a; 
        }
    }
    char* path = construct_path(filename, OUTPUT_DIRECTORY);

    if (stbi_write_png(path, width, height, 4, rgb_image, width * 4) == 0) {
        printf("Error: Failed to write the PNG image.\n");
    }


    free(rgb_image);
    free(path);
}

void create_luminance_image(char* filename,uint8_t** values, ImageData data) {
    Pixel **pixels = malloc(data.height * sizeof(Pixel *));
    for (int y = 0; y < data.height; y++) {
        pixels[y] = malloc(data.width * sizeof(Pixel));
    }


    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            pixels[y][x].r = values[y][x];
            pixels[y][x].g = values[y][x];
            pixels[y][x].b = values[y][x];
            pixels[y][x].a = 255;  
        }
    }


    create_png_image(filename, data.width, data.height, pixels);


    free_pixels(pixels, data.height);
}

void print_pixel_value(uint8_t** values, ImageData data){
    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            printf("value at (%zu;%zu): %u\n", y,x,values[y][x]);
        }
    }
}

void create_rChrominance_image(char* filename,uint8_t** values, ImageData data) {
    Pixel **pixels = malloc(data.height * sizeof(Pixel *));
    for (int y = 0; y < data.height; y++) {
        pixels[y] = malloc(data.width * sizeof(Pixel));
    }


          for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            pixels[y][x].r = 128+1.402*(values[y][x]-128);
            pixels[y][x].g = 128-0.344*(128-128)-0.714*(values[y][x]-128);
            pixels[y][x].b = 128 + 1.772*(128-128);
            pixels[y][x].a = 255; 
        }
    }


    create_png_image(filename, data.width, data.height, pixels);

    free_pixels(pixels, data.height);
}

void create_bChrominance_image(char* filename,uint8_t** values, ImageData data) {
    Pixel **pixels = malloc(data.height * sizeof(Pixel *));
    for (int y = 0; y < data.height; y++) {
        pixels[y] = malloc(data.width * sizeof(Pixel));
    }

        for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            pixels[y][x].r = 128+1.402*(128-128);
            pixels[y][x].g = 128-0.344*(values[y][x]-128)-0.714*(128-128);
            pixels[y][x].b = 128 + 1.772*(values[y][x]-128);
            pixels[y][x].a = 255; 
        }
    }

    create_png_image(filename, data.width, data.height, pixels);


    free_pixels(pixels, data.height);
}

void chroma_subsample(uint8_t*** values, ImageData data) {
   printf("Subsampling the chrominance matrix...\n");
    uint8_t** sub_sample = malloc(sizeof(uint8_t*) * data.height);
    if (!sub_sample) {

        return;
    }

    for (int y = 0; y < data.height; y++) {
        sub_sample[y] = malloc(data.width / 2 * sizeof(uint8_t));
        if (!sub_sample[y]) {
 
            for (int i = 0; i < y; i++) {
                free(sub_sample[i]);
            }
            free(sub_sample);
            return;
        }
    }

    
    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 1; x < data.width; x += 2) {
            sub_sample[y][x / 2] = (*values)[y][x];
        }
    }


    for (int y = 0; y < data.height; y++) {
        free((*values)[y]);
    }
    free(*values); 

    
    *values = malloc(sizeof(uint8_t*) * data.height);
    if (!(*values)) {
        return;
    }

    for (int y = 0; y < data.height; y++) {
        (*values)[y] = malloc(data.width / 2 * sizeof(uint8_t));
        if (!(*values)[y]) {
            
            for (int i = 0; i < y; i++) {
                free((*values)[i]);
            }
            free(*values);
            return;
        }
    }

    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width / 2; x++) {
            (*values)[y][x] = sub_sample[y][x]; 
        }
    }


    for (int y = 0; y < data.height; y++) {
        free(sub_sample[y]);
    }
    free(sub_sample);
}



double calculate_mse(uint8_t** original, Pixel** reconstructed, size_t rows, size_t cols) {
    double mse = 0.0;

    // Loop over all the pixels in the image (rows x cols)
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            double diff = (double)original[i][j] - reconstructed[i][j].r;  // Get difference for each pixel
            mse += diff * diff;  // Sum squared differences
        }
    }

    mse /= (rows * cols);  // Average the squared differences

    // Print the MSE
    printf("Mean Squared Error (MSE): %f\n", mse);

    return mse;
}


void inverse_discrete_cosine_transform(uint8_t* values, size_t width, size_t height, double* coefficients) {
    double* temp_values = malloc(width * height * sizeof(double));
    if (!temp_values) {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Initialize the output block to zero
    for (size_t i = 0; i < width * height; i++) {
        temp_values[i] = 0.0;
    }

    // Perform 2D IDCT-II
    for (size_t x = 0; x < height; x++) {
        for (size_t y = 0; y < width; y++) {
            double sum = 0.0;

            for (size_t u = 0; u < height; u++) {
                for (size_t v = 0; v < width; v++) {
                    double alpha_u = (u == 0) ? sqrt(1.0 / height) : sqrt(2.0 / height);
                    double alpha_v = (v == 0) ? sqrt(1.0 / width) : sqrt(2.0 / width);
                    double cos_x = cos((PI * (2 * x + 1) * u) / (2.0 * height));
                    double cos_y = cos((PI * (2 * y + 1) * v) / (2.0 * width));

                    sum += alpha_u * alpha_v * coefficients[u * width + v] * cos_x * cos_y;
                }
            }

            // Adjust for chrominance channel scaling
            temp_values[x * width + y] = sum;
        }
    }

    // Convert to uint8_t and clamp to [0, 255]
    for (size_t i = 0; i < width * height; i++) {
        int value = (int)round(temp_values[i] + 128.0);  // Shift back to [0, 255] range
        values[i] = (value < 0) ? 0 : (value > 255) ? 255 : (uint8_t)value;
    }

    free(temp_values);
}

// Perform Discrete Cosine Transform (DCT) with chrominance ratio consideration
void discrete_cosine_transform(uint8_t* data, size_t width, size_t height, double** coefficients) {
    *coefficients = malloc(width * height * sizeof(double));
    int* corrected_values = malloc(width * height * sizeof(int));

    if (!*coefficients || !corrected_values) {
        free(*coefficients);
        free(corrected_values);
        fprintf(stderr, "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Shift input data range from [0, 255] to [-128, 127]
    for (size_t i = 0; i < width * height; i++) {
        corrected_values[i] = (int)data[i] - 128;
    }

    // Perform the 2D DCT-II
    for (size_t u = 0; u < height; u++) {
        for (size_t v = 0; v < width; v++) {
            double sum = 0.0;

            for (size_t x = 0; x < height; x++) {
                for (size_t y = 0; y < width; y++) {
                    double cos_x = cos((PI * (2 * x + 1) * u) / (2.0 * height));
                    double cos_y = cos((PI * (2 * y + 1) * v) / (2.0 * width));
                    sum += corrected_values[x * width + y] * cos_x * cos_y;
                }
            }

            double alpha_u = (u == 0) ? sqrt(1.0 / height) : sqrt(2.0 / height);
            double alpha_v = (v == 0) ? sqrt(1.0 / width) : sqrt(2.0 / width);
            (*coefficients)[u * width + v] = alpha_u * alpha_v * sum;
        }
    }

    free(corrected_values);
}





PixelGroup* divide_image(uint8_t** luminance_values, uint8_t** rChrominance_values, uint8_t** bChrominance_values, ImageData data, size_t group_size) {
    size_t blocks_per_row = (data.width + group_size - 1) / group_size;  
    size_t blocks_per_col = (data.height + group_size - 1) / group_size; 
    size_t group_count = blocks_per_row * blocks_per_col;

    printf("Group count: %zu, Pixel count: %zu, Blocks per row: %zu, Blocks per col: %zu\n", 
           group_count, data.pixel_count, blocks_per_row, blocks_per_col);

    PixelGroup* groups = malloc(group_count * sizeof(PixelGroup));
    if (!groups) {
        perror("Failed to allocate memory for groups");
        return NULL;
    }

    for (size_t i = 0; i < group_count; i++) {
        for (size_t j = 0; j < group_size * group_size; j++) {
            groups[i].lum_values[j] = 0; // Initialize luminance
        }
        for (size_t j = 0; j < group_size * (group_size / 2); j++) {
            groups[i].r_values[j] = 0; // Initialize R chrominance
            groups[i].b_values[j] = 0; // Initialize B chrominance
        }
    }

    for (size_t row = 0; row < data.height; row++) {
        for (size_t col = 0; col < data.width; col++) {
            size_t block_row = row / group_size;
            size_t block_col = col / group_size;
            size_t block_index = block_row * blocks_per_row + block_col;

            size_t local_row = row % group_size;
            size_t local_col = col % group_size;
            size_t local_index = local_row * group_size + local_col;

            groups[block_index].lum_values[local_index] = luminance_values[row][col];

            // For 4:2:2 chrominance subsampling
            if (local_col % 2 == 0) { // Process chrominance for even columns
                size_t chroma_index = local_row * (group_size / 2) + (local_col / 2);
                groups[block_index].r_values[chroma_index] = rChrominance_values[row][col / 2];
                groups[block_index].b_values[chroma_index] = bChrominance_values[row][col / 2];
            }
        }
    }

    return groups;
}

void assemble_image(ImageData* output, ImageData original, PixelGroup* groups) {
    output->height = original.height;
    output->width = original.width;
    output->pixel_count = original.pixel_count;

    // Allocate memory for the pixel array
    output->pixels = malloc(output->height * sizeof(Pixel*));
    for (int y = 0; y < output->height; y++) {
        output->pixels[y] = malloc(output->width * sizeof(Pixel));
    }

    const size_t group_size = 8;
    const size_t blocks_per_row = (original.width + group_size - 1) / group_size;
    const size_t blocks_per_col = (original.height + group_size - 1) / group_size;

    for (size_t block_row = 0; block_row < blocks_per_col; block_row++) {
        for (size_t block_col = 0; block_col < blocks_per_row; block_col++) {
            size_t block_index = block_row * blocks_per_row + block_col;

            for (size_t local_row = 0; local_row < group_size; local_row++) {
                for (size_t local_col = 0; local_col < group_size; local_col++) {
                    size_t global_row = block_row * group_size + local_row;
                    size_t global_col = block_col * group_size + local_col;

                    if (global_row < output->height && global_col < output->width) {
                        size_t local_index = local_row * group_size + local_col;

                        // Get luminance value
                        uint8_t Y = groups[block_index].lum_values[local_index];

                        // Correct chroma indexing for 4:2:2
                        size_t chroma_col = local_col / 2;
                        size_t chroma_index = local_row * (group_size / 2) + chroma_col;

                        // Retrieve chrominance values
                        uint8_t Cb = groups[block_index].b_values[chroma_index];
                        uint8_t Cr = groups[block_index].r_values[chroma_index];

                        // Convert YCbCr to RGB
                        int R = (int)Y + (int)(1.402 * (Cr - 128));
                        int G = (int)Y - (int)(0.344136 * (Cb - 128)) - (int)(0.714136 * (Cr - 128));
                        int B = (int)Y + (int)(1.772 * (Cb - 128));

                        // Clamp values to [0, 255]
                        R = R < 0 ? 0 : (R > 255 ? 255 : R);
                        G = G < 0 ? 0 : (G > 255 ? 255 : G);
                        B = B < 0 ? 0 : (B > 255 ? 255 : B);

                        // Assign to output pixel
                        output->pixels[global_row][global_col].r = (uint8_t)R;
                        output->pixels[global_row][global_col].g = (uint8_t)G;
                        output->pixels[global_row][global_col].b = (uint8_t)B;
                        output->pixels[global_row][global_col].a = 255;

                        // Debugging output
                     
                    }
                }
            }
        }
    }
}



void Quantize(double** luminance, size_t* table, size_t size){
    
    for(size_t i = 0; i< size; i++){
        (*luminance)[i] /= table[i];
        (*luminance)[i] = (int)(*luminance)[i];
    }
}

void Inverse_quantize(double** luminance, size_t* table, size_t size){
    
    for(size_t i = 0; i< size; i++){
        (*luminance)[i] *= table[i];
    }
}

void reconstruct_chrominance_matrix(PixelGroup* blocks, uint8_t*** chrominance_matrix, ImageData image, char channel) {
    size_t block_index = 0;

    // Allocate memory for the reconstructed chrominance matrix
    *chrominance_matrix = (uint8_t**)malloc(image.height * sizeof(uint8_t*));
    for (size_t i = 0; i < image.height; i++) {
        (*chrominance_matrix)[i] = (uint8_t*)malloc(image.width * sizeof(uint8_t));
        memset((*chrominance_matrix)[i], 0, image.width * sizeof(uint8_t)); // Initialize to zero
    }

    // Loop through the image in 8x8 blocks (this loop handles the actual block-by-block reconstruction)
    for (size_t block_row = 0; block_row < (image.height + 7) / 8; block_row++) {
        for (size_t block_col = 0; block_col < (image.width + 7) / 8; block_col++) {
            PixelGroup* block = &blocks[block_index++];

            // For the given block, reconstruct the chrominance values
            for (size_t local_row = 0; local_row < 8; local_row++) {
                size_t global_row = block_row * 8 + local_row;

                if (global_row >= image.height) {
                    break;
                }

                for (size_t local_col = 0; local_col < 4; local_col++) {
                    size_t global_col = block_col * 8 + local_col * 2;

                    if (global_col + 1 >= image.width) {
                        break;
                    }

                    // Calculate chroma index for subsampled chrominance data
                    size_t chroma_index = local_row * 4 + local_col;

                    // Retrieve chrominance value from the block
                    uint8_t chroma_value = (channel == 'R') ? block->r_values[chroma_index] : block->b_values[chroma_index];

                    // Assign chrominance value to two adjacent pixels
                    (*chrominance_matrix)[global_row][global_col] = chroma_value;
                    (*chrominance_matrix)[global_row][global_col + 1] = chroma_value;
                }
            }
        }
    }
}


void zigzag_pattern(size_t width, size_t height, double* input, double* output) {
    size_t index = 0;

    for (size_t sum = 0; sum < width + height - 1; sum++) {
        // Calculate start and end rows based on the sum
        size_t start_row = (sum < width) ? 0 : sum - width + 1;
        size_t end_row = (sum < height) ? sum : height - 1;

        if (sum % 2 == 0) {
            // Traverse the diagonal in reverse if sum is even
            for (size_t row = end_row; row >= start_row && row < height; row--) {
                size_t col = sum - row;
                if (col < width) {
                    output[index++] = input[row * width + col];
                }
            }
        } else {
            // Traverse the diagonal normally if sum is odd
            for (size_t row = start_row; row <= end_row; row++) {
                size_t col = sum - row;
                if (col < width) {
                    output[index++] = input[row * width + col];
                }
            }
        }
    }
}
void reverse_zigzag_pattern(size_t width, size_t height, double* input, double* output) {
    size_t index = 0;

    for (size_t sum = 0; sum < width + height - 1; sum++) {
        // Calculate start and end rows for this diagonal
        size_t start = (sum < height) ? 0 : sum - height + 1;
        size_t end = (sum < width) ? sum : height - 1;

        if (sum % 2 == 0) {
            // Even sum: traverse from bottom to top
            for (size_t row = end; row >= start && row < height; row--) {
                size_t col = sum - row;
                if (col < width) {
                    output[row * width + col] = input[index++];
                }
            }
        } else {
            // Odd sum: traverse from top to bottom
            for (size_t row = start; row <= end && row < height; row++) {
                size_t col = sum - row;
                if (col < width) {
                    output[row * width + col] = input[index++];
                }
            }
        }
    }
}

// RLE Encoding: Input array -> Encoded as [count, value] pairs
void RLE(double* input, size_t length, int** output, size_t* out_length) {
    if (length == 0) {
        *output = NULL;
        *out_length = 0;
        return;
    }

    // Allocate memory for worst-case scenario (each element is distinct)
    *output = (int*)malloc(sizeof(int) * 2 * length);
    if (!*output) {
        perror("Failed to allocate memory for RLE output.");
        return;
    }

    size_t out_index = 0;
    double current_value = input[0];
    size_t count = 1;

    for (size_t i = 1; i <= length; i++) {
        if (i < length && (int)input[i] == (int)current_value) {
            count++;
        } else {
            // Store count and value as consecutive integers
            (*output)[out_index++] = (int)count;
            (*output)[out_index++] = (int)current_value;

            if (i < length) {
                current_value = input[i];
                count = 1;
            }
        }
    }

    *out_length = out_index;
}

void inverse_RLE(int* input, double* output, size_t max_size, size_t in_length) {
    size_t index = 0;
    for (size_t i = 0; i < in_length; i += 2) {
        
        int count = input[i];
        int value = input[i + 1];
        //printf("Adding %d %d times\n", value, count);

        // Ensure count doesn't exceed array size
        if (index + count > max_size) {
            count = max_size - index;
        }

        // Fill in the output array with the repeated value
        for (int j = 0; j < count; j++) {
            if (index < max_size) {
                output[index++] = (double)value;
            }
        }
    }

    // Fill any remaining space with zeros if index didn't reach max_size
    while (index < max_size) {
        output[index++] = 0;
    }
}

typedef struct {
    int count;
    int value;
} frequency;

typedef struct Node {
int count;
int value;
struct Node* left;
struct Node* right;
} Node;

typedef struct {
int value;
char code[32];
} HuffmanCode;

void calculate_frequency(frequency* frequencies, size_t input_len, int* input, size_t* unique_count) {
for (size_t i = 0; i < input_len; i++) {
    int found = 0;
    for (size_t j = 0; j < (*unique_count); j++) {
        if (frequencies[j].value == input[i] + 1000) {
            frequencies[j].count++;
            found = 1;
            break;
        }
    }
    if (!found) {
        frequencies[(*unique_count)].value = input[i] + 1000;
        frequencies[(*unique_count)].count = 1;
        (*unique_count)++;
    }
}
}

void swap(Node* a, Node* b) {
Node temp = *a;
*a = *b;
*b = temp;
}

void heapify(Node* heap, size_t heap_size, size_t i) {
size_t smallest = i;
size_t left = 2 * i + 1;
size_t right = 2 * i + 2;

if (left < heap_size && heap[left].count < heap[smallest].count)
    smallest = left;

if (right < heap_size && heap[right].count < heap[smallest].count)
    smallest = right;

if (smallest != i) {
    swap(&heap[i], &heap[smallest]);
    heapify(heap, heap_size, smallest);
}
}

void build_heap(frequency* frequencies, size_t unique_count, Node** heap) {
*heap = malloc(sizeof(Node) * unique_count);
if (!*heap) {
    printf("Memory allocation failed for heap\n");
    return;
}

for (size_t i = 0; i < unique_count; i++) {
    (*heap)[i].count = frequencies[i].count;
    (*heap)[i].value = frequencies[i].value;
    (*heap)[i].left = NULL;
    (*heap)[i].right = NULL;
}

for (int i = (int)(unique_count / 2) - 1; i >= 0; i--) {
    heapify(*heap, unique_count, i);
}
}

Node* build_huffman_tree(Node* heap, size_t* heap_size) {
while (*heap_size > 1) {
    Node left = heap[0];
    heap[0] = heap[--(*heap_size)];
    heapify(heap, *heap_size, 0);

    Node right = heap[0];
    heap[0] = heap[--(*heap_size)];
    heapify(heap, *heap_size, 0);

    Node* new_node = malloc(sizeof(Node));
    new_node->count = left.count + right.count;
    new_node->value = -1;
    new_node->left = malloc(sizeof(Node));
    *new_node->left = left;
    new_node->right = malloc(sizeof(Node));
    *new_node->right = right;

    heap[*heap_size] = *new_node;
    (*heap_size)++;
    heapify(heap, *heap_size, (*heap_size) - 1);
}
return &heap[0];
}

void assign_codes(Node* root, char* code, int depth, HuffmanCode* codes, int* code_index) {
if (!root) return;

if (root->value != -1) {
    codes[*code_index].value = root->value;
    code[depth] = '\0';
    strcpy(codes[*code_index].code, code);
    (*code_index)++;
    return;
}

code[depth] = '0';
assign_codes(root->left, code, depth + 1, codes, code_index);

code[depth] = '1';
assign_codes(root->right, code, depth + 1, codes, code_index);
}

void print_codes(HuffmanCode* codes, int code_count) {
printf("Huffman Codes:\n");
for (int i = 0; i < code_count; i++) {
    printf("Value: %d -> Code: %s\n", codes[i].value - 1000, codes[i].code);
}
}

void generate_encoded_sequence(int* input, size_t input_len, HuffmanCode* codes, int code_count, char* encoded_sequence) {
encoded_sequence[0] = '\0';
for (size_t i = 0; i < input_len; i++) {
    for (int j = 0; j < code_count; j++) {
        if (codes[j].value == input[i] + 1000) {
            strcat(encoded_sequence, codes[j].code);
            break;
        }
    }
}
}

double* decode_huffman(Node* root, const char* encoded_sequence, size_t* decoded_len) {
Node* current_node = root;
size_t capacity = 100;
double* decoded_output = malloc(sizeof(double) * capacity);
size_t count = 0;

for (size_t i = 0; encoded_sequence[i] != '\0'; i++) {
    current_node = (encoded_sequence[i] == '0') ? current_node->left : current_node->right;

    if (!current_node->left && !current_node->right) {
        if (count == capacity) {
            capacity *= 2;
            decoded_output = realloc(decoded_output, sizeof(double) * capacity);
        }
        decoded_output[count++] = (double)(current_node->value - 1000);
        current_node = root;
    }
}

*decoded_len = count;
return decoded_output;
}

HuffmanCode* encode_huffman(int* input, size_t input_len, size_t* code_count, Node** root_out) {
frequency* frequencies = malloc(sizeof(frequency) * input_len);
size_t unique_count = 0;
calculate_frequency(frequencies, input_len, input, &unique_count);

Node* heap = NULL;
build_heap(frequencies, unique_count, &heap);

size_t heap_size = unique_count;
Node* root = build_huffman_tree(heap, &heap_size);
*root_out = root;

HuffmanCode* codes = malloc(sizeof(HuffmanCode) * unique_count);
int code_index = 0;
char code[32];
assign_codes(root, code, 0, codes, &code_index);
*code_count = code_index;

//print_codes(codes, code_index);

free(frequencies);
free(heap);

return codes;
}






int main() {
    printf("Constructing file path...\n");
    char* path = construct_path("og.png", IMAGES_DIRECTORY);
    ImageData image = read_image(path);
    free(path);
    
    printf("Creating initial PNG image from input data...\n");
    create_png_image("original.png", image.width, image.height, image.pixels);

    printf("Building luminance matrix...\n");
    uint8_t** luminance_matrix;
    build_luminance_matrix(image, &luminance_matrix);

    printf("Building red chrominance matrix...\n");
    uint8_t** rChrominance_matrix;
    build_rChrominance_matrix(image, &rChrominance_matrix);

    printf("Building blue chrominance matrix...\n");
    uint8_t** bChrominance_matrix;
    build_bChrominance_matrix(image, &bChrominance_matrix);

    create_bChrominance_image("bChrominance.png", bChrominance_matrix, image);
    create_rChrominance_image("rChrominance.png", rChrominance_matrix, image);
    create_luminance_image("luminance.png", luminance_matrix, image);

    printf("Performing chroma subsampling on blue chrominance...\n");
    chroma_subsample(&bChrominance_matrix, image);

    printf("Performing chroma subsampling on red chrominance...\n");
    chroma_subsample(&rChrominance_matrix, image);

    
    
    size_t total_blocks = (size_t)ceil((double)image.pixel_count / 64);
    printf("Dividing input into 8x8 blocks (Total blocks: %zu)...\n", total_blocks);
    PixelGroup* blocks = divide_image(luminance_matrix, rChrominance_matrix, bChrominance_matrix, image, 8);

    printf("Computing DCT-II coefficients for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        discrete_cosine_transform(blocks[i].lum_values, 8, 8, &(blocks[i].lum_coefficients));
        discrete_cosine_transform(blocks[i].r_values, 4, 8, &(blocks[i].r_coefficients));
        discrete_cosine_transform(blocks[i].b_values, 4, 8, &(blocks[i].b_coefficients));
    
    }

   

    printf("Quantizing the matrices for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        Quantize(&(blocks[i].lum_coefficients), LUMINANCE_QUANTIZATION_TABLE, 64);
        Quantize(&(blocks[i].b_coefficients), CHROMINANCE_QUANTIZATION_TABLE, 32);
        Quantize(&(blocks[i].r_coefficients), CHROMINANCE_QUANTIZATION_TABLE, 32);
    }

    
    //     for(size_t n = 0; n<64;n++){
    //         printf("%d ", (int)blocks[0].lum_coefficients[n]);
    //         if((n+1)%8==0){
    //             printf("\n");
    //         }
    //     }
    // printf("\n");

    for (size_t i = 0; i < total_blocks; i++) {
        // printf("First normal chrominance block:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)blocks[0].r_coefficients[j]);
        // }
        // printf("\n");

        // printf("First normal luminance block:\n");
        // for(size_t j =0;j<64;j++){
        //     printf("%d ", (int)blocks[0].lum_coefficients[j]);
        // }
        // printf("\n");

        double transformed_lum[64];
        double transformed_r[32];
        double transformed_b[32]; // temporary array for zigzagged coefficients
        zigzag_pattern(8, 8, blocks[i].lum_coefficients, transformed_lum);
        zigzag_pattern(4, 8, blocks[i].r_coefficients, transformed_r);
        zigzag_pattern(4, 8, blocks[i].b_coefficients, transformed_b);

        // printf("After zigzag - luminance:\n");
        // for(size_t j =0;j<64;j++){
        //     printf("%d ", (int)transformed_lum[j]);
        // }
        // printf("\n");

        // printf("After zigzag - r chrominance:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)transformed_r[j]);
        // }
        // printf("\n");

        // printf("After zigzag - b chrominance:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)transformed_b[j]);
        // }
        // printf("\n");

        // Copy the transformed data back to the block's lum_coefficients
        for (size_t j = 0; j < 64; j++) {
            blocks[i].lum_coefficients[j] = transformed_lum[j];
        }
        for(size_t j =0;j<32;j++){
            blocks[i].r_coefficients[j] = transformed_r[j];
            blocks[i].b_coefficients[j] = transformed_b[j];
        }
        // printf("First zigzaged chrominance block:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)blocks[0].r_coefficients[j]);
        // }
        // printf("\n");

        size_t encoded_length_lum;
        size_t encoded_length_r;
        size_t encoded_length_b;
        
        RLE(transformed_lum, 64, &(blocks[i].RLE_encoded_lum), &encoded_length_lum);
        RLE(transformed_r, 32, &(blocks[i].RLE_encoded_r), &encoded_length_r);
        RLE(transformed_b, 32, &(blocks[i].RLE_encoded_b), &encoded_length_b);
        // printf("RLE Encoded luminance (count, value): ");
        // for (size_t j = 0; j < encoded_length_lum; j+=2) {
        //     printf("(%d, %d) ", (int)blocks[i].RLE_encoded_lum[j], (int)blocks[i].RLE_encoded_lum[j+1]);
        // }
        // printf("\n");

        // printf("RLE Encoded r chrominance (count, value): ");
        // for (size_t j = 0; j < encoded_length_r; j+=2) {
        //     printf("(%d, %d) ", (int)blocks[i].RLE_encoded_r[j], (int)blocks[i].RLE_encoded_r[j+1]);
        // }
        // printf("\n");

        // printf("RLE Encoded b chrominance (count, value): ");
        // for (size_t j = 0; j < encoded_length_b; j+=2) {
        //     printf("(%d, %d) ", (int)blocks[i].RLE_encoded_b[j], (int)blocks[i].RLE_encoded_b[j+1]);
        // }
        // printf("\n");



        
    
    // Encode and decode luminance (64 elements)
size_t code_count_lum;
Node* root_lum;
HuffmanCode* codes_lum = encode_huffman(blocks[i].RLE_encoded_lum, encoded_length_lum, &code_count_lum, &root_lum);
// Print Huffman codes for Luminance
// printf("\nHuffman Codes for Luminance:\n");
// for (size_t j = 0; j < code_count_lum; j++) {
//     printf("Value: %d -> Code: %s\n", codes_lum[j].value, codes_lum[j].code);
// }
char encoded_sequence_lum[1024];
generate_encoded_sequence(blocks[i].RLE_encoded_lum, encoded_length_lum, codes_lum, code_count_lum, encoded_sequence_lum);
//printf("Encoded Luminance Sequence: %s\n", encoded_sequence_lum);

size_t decoded_len_lum;
double* decoded_output_lum = decode_huffman(root_lum, encoded_sequence_lum, &decoded_len_lum);

// printf("Decoded Luminance Output: ");
// for (size_t j = 0; j < decoded_len_lum; j+=2) {
//     printf("(%d, %d) ", (int)decoded_output_lum[j], (int)decoded_output_lum[j+1]);
// }
// printf("\n");

// for (size_t j = 0; j < decoded_len_lum; j++) {
//     blocks[i].RLE_encoded_lum[j] = (int)decoded_output_lum[j];
// }


free(codes_lum);
free(decoded_output_lum);

// // // Encode and decode R chrominance (32 elements)
size_t code_count_r;
Node* root_r;
HuffmanCode* codes_r = encode_huffman(blocks[i].RLE_encoded_r, encoded_length_r, &code_count_r, &root_r);

char encoded_sequence_r[512];
generate_encoded_sequence(blocks[i].RLE_encoded_r, encoded_length_r, codes_r, code_count_r, encoded_sequence_r);
// printf("Encoded R Chrominance Sequence: %s\n", encoded_sequence_r);

size_t decoded_len_r;
double* decoded_output_r = decode_huffman(root_r, encoded_sequence_r, &decoded_len_r);

// printf("Decoded R Chrominance Output: ");
// for (size_t j = 0; j < decoded_len_r; j++) {
//     printf("%.2f ", decoded_output_r[j]);
// }
// printf("\n");

for (size_t j = 0; j < 64; j++) {
    blocks[i].RLE_encoded_r[j] = (int)decoded_output_r[j];
}

free(codes_r);
free(decoded_output_r);

// Encode and decode B chrominance (32 elements)
size_t code_count_b;
Node* root_b;
HuffmanCode* codes_b = encode_huffman(blocks[i].RLE_encoded_b, encoded_length_b, &code_count_b, &root_b);

char encoded_sequence_b[512];
generate_encoded_sequence(blocks[i].RLE_encoded_b, encoded_length_b, codes_b, code_count_b, encoded_sequence_b);
// printf("Encoded B Chrominance Sequence: %s\n", encoded_sequence_b);

size_t decoded_len_b;
double* decoded_output_b = decode_huffman(root_b, encoded_sequence_b, &decoded_len_b);

// printf("Decoded B Chrominance Output: ");
// for (size_t j = 0; j < decoded_len_b; j++) {
//     printf("%.2f ", decoded_output_b[j]);
// }
// printf("\n");
for (size_t j = 0; j < 64; j++) {
    blocks[i].RLE_encoded_b[j] = (int)decoded_output_b[j];
}

free(codes_b);
free(decoded_output_b);









        inverse_RLE(blocks[i].RLE_encoded_lum, blocks[i].lum_coefficients, 64, encoded_length_lum);
         inverse_RLE(blocks[i].RLE_encoded_r, blocks[i].r_coefficients, 32, encoded_length_r);
         inverse_RLE(blocks[i].RLE_encoded_b, blocks[i].b_coefficients, 32, encoded_length_b);

        // printf("After inverse RLE - luminance:\n");
        // for(size_t j =0;j<64;j++){
        //     printf("%d ", (int)blocks[i].lum_coefficients[j]);
        // }
        // printf("\n");

        // printf("After inverse RLE - r chrominance:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)blocks[i].r_coefficients[j]);
        // }
        // printf("\n");

        // printf("After inverse RLE - b chrominance:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)blocks[i].b_coefficients[j]);
        // }
        // printf("\n");

        double reverse_transformed_lum[64];
        double reverse_transformed_r[32];
        double reverse_transformed_b[32]; // temporary array for reverse zigzagged coefficients
        reverse_zigzag_pattern(8, 8, blocks[i].lum_coefficients, reverse_transformed_lum);
        reverse_zigzag_pattern(4, 8, blocks[i].r_coefficients, reverse_transformed_r);
        reverse_zigzag_pattern(4, 8, blocks[i].b_coefficients, reverse_transformed_b);

        // printf("After reverse zigzag - luminance:\n");
        // for(size_t j =0;j<64;j++){
        //     printf("%d ", (int)reverse_transformed_lum[j]);
        // }
        // printf("\n");

        // printf("After reverse zigzag - r chrominance:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)reverse_transformed_r[j]);
        // }
        // printf("\n");

        // printf("After reverse zigzag - b chrominance:\n");
        // for(size_t j =0;j<32;j++){
        //     printf("%d ", (int)reverse_transformed_b[j]);
        // }
        // printf("\n");

        // Copy the reverse-transformed data back to the block's lum_coefficients
        for (size_t j = 0; j < 64; j++) {
            blocks[i].lum_coefficients[j] = reverse_transformed_lum[j];
        }
        for (size_t j = 0; j < 32; j++) {
            blocks[i].r_coefficients[j] = reverse_transformed_r[j];
            blocks[i].b_coefficients[j] = reverse_transformed_b[j];
        }
    }

        
        
    printf("Inverse-quantizing the matrices for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        Inverse_quantize(&(blocks[i].lum_coefficients), LUMINANCE_QUANTIZATION_TABLE, 64);
        Inverse_quantize(&(blocks[i].b_coefficients), CHROMINANCE_QUANTIZATION_TABLE, 32);
        Inverse_quantize(&(blocks[i].r_coefficients), CHROMINANCE_QUANTIZATION_TABLE, 32);
    }

    printf("Performing Inverse-DCT on the values for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        inverse_discrete_cosine_transform(blocks[i].lum_values, 8, 8, blocks[i].lum_coefficients);
        inverse_discrete_cosine_transform(blocks[i].b_values, 4, 8, blocks[i].b_coefficients);
        inverse_discrete_cosine_transform(blocks[i].r_values, 4, 8, blocks[i].r_coefficients);
    }

    printf("Assembling the reconstructed image...\n");
    ImageData new_image = {0};
    assemble_image(&new_image, image, blocks);

    printf("Saving the reconstructed PNG image...\n");
    create_png_image("reconstructed.png", new_image.width, new_image.height, new_image.pixels);

    // **Reconstruct chrominance matrices**
    printf("Reconstructing blue chrominance matrix...\n");
    uint8_t** reconstructed_bChrominance_matrix;
    reconstruct_chrominance_matrix(blocks, &reconstructed_bChrominance_matrix, image, 'B');
    create_bChrominance_image("reconstructed_bChrominance.png", reconstructed_bChrominance_matrix, image);

    printf("Reconstructing red chrominance matrix...\n");
    uint8_t** reconstructed_rChrominance_matrix;
    reconstruct_chrominance_matrix(blocks, &reconstructed_rChrominance_matrix, image, 'R');
    create_rChrominance_image("reconstructed_rChrominance.png", reconstructed_rChrominance_matrix, image);

    

    printf("Calculating Mean Squared Error (MSE)...\n");
    calculate_mse(luminance_matrix, new_image.pixels, new_image.height, new_image.width);

    printf("Freeing allocated memory...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        free(blocks[i].lum_coefficients);
        free(blocks[i].b_coefficients);
        free(blocks[i].r_coefficients);
        free(blocks[i].RLE_encoded_lum);
        free(blocks[i].RLE_encoded_r);
        free(blocks[i].RLE_encoded_b);
    }
    free(blocks);
    free_pixels(image.pixels, image.height);
    free_pixels(new_image.pixels, new_image.height);


    printf("Program completed successfully.\n");
    return 0;
}

