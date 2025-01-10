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




size_t CHROMINANCE_QUANTIZATION_TABLE [64] = {
17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
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

void build_rChrominance_matrix(ImageData data, uint8_t*** matrix) {
    printf("Building rChrominance matrix...\n");
    *matrix = malloc(data.height * sizeof(uint8_t*));

    for (int y = 0; y < data.height; y++) {
        (*matrix)[y] = malloc(data.width * sizeof(uint8_t));
    }

    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            (*matrix)[y][x] = -0.1687 * data.pixels[y][x].r - 0.3313 * data.pixels[y][x].g + 0.5 * data.pixels[y][x].b;
        }
    }
}

void build_bChrominance_matrix(ImageData data, uint8_t*** matrix) {
    printf("Building bChrominance matrix...\n");
    *matrix = malloc(data.height * sizeof(uint8_t*));

    for (int y = 0; y < data.height; y++) {
        (*matrix)[y] = malloc(data.width * sizeof(uint8_t));
    }

    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            (*matrix)[y][x] = 0.5 * data.pixels[y][x].r - 0.4187 * data.pixels[y][x].g - 0.813 * data.pixels[y][x].b;
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
            pixels[y][x].r = values[y][x];
            pixels[y][x].g = 0;
            pixels[y][x].b = 0;
            pixels[y][x].a = 255; 
        }
    }


    create_png_image(filename, data.width/2, data.height, pixels);

    free_pixels(pixels, data.height);
}

void create_bChrominance_image(char* filename,uint8_t** values, ImageData data) {
    Pixel **pixels = malloc(data.height * sizeof(Pixel *));
    for (int y = 0; y < data.height; y++) {
        pixels[y] = malloc(data.width * sizeof(Pixel));
    }

    create_png_image(filename, data.width/2, data.height, pixels);


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


void inverse_discrete_cosine_transform(uint8_t (*values)[64], size_t data_length, double* coefficients) {

    // Temporary array to store calculated values
    double temp_values[data_length];
    for (size_t n = 0; n < data_length; n++) {
        temp_values[n] = 0.0;
    }

    // Perform the IDCT
    for (size_t n = 0; n < data_length; n++) {
        for (size_t k = 0; k < data_length; k++) {
            double scaling = (k == 0) ? sqrt(1.0 / data_length) : sqrt(2.0 / data_length);
            temp_values[n] += coefficients[k] * cos((PI / data_length) * k * (n + 0.5)) * scaling;
        }
    }

     // Convert to uint8_t and clamp
    for (size_t n = 0; n < data_length; n++) {
        int value = (int)(temp_values[n] + 128); // Add 128 for unsigned representation
        if (value < 0) value = 0;
        if (value > 255) value = 255;
        (*values)[n] = (uint8_t)value;
    }
}


void discrete_cosine_transform(uint8_t* data, size_t data_length, double** coefficients) {

    *coefficients = malloc(data_length * sizeof(double));
    int* corrected_values = malloc(data_length *sizeof(int));

    if (*coefficients == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }
 

  for (size_t i = 0; i < data_length; i++) {
    int corrected_value = (int)data[i] - 128; 
    corrected_values[i] = corrected_value; 
    }


    for (size_t k = 0; k < data_length; k++) {
    (*coefficients)[k] = 0.0;
}

    for (size_t k = 0; k < data_length; k++) {
        for (size_t n = 0; n < data_length; n++) {
            (*coefficients)[k] += ((double)corrected_values[n]) * cos((PI / data_length) * (n + 0.5) * k);
        }

        if(k == 0){
            (*coefficients)[k] *= sqrt(1.0 / data_length);
        }else{
            (*coefficients)[k] *= sqrt(2.0 / data_length);
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
                        uint8_t luminance = groups[block_index].lum_values[local_index];

                        output->pixels[global_row][global_col].r = luminance;
                        output->pixels[global_row][global_col].g = luminance;
                        output->pixels[global_row][global_col].b = luminance;
                        output->pixels[global_row][global_col].a = 255; 
                    }
                }
            }
        }
    }
}

void Quantize(double** luminance){
    
    for(size_t i = 0; i< 64; i++){
        (*luminance)[i] /= LUMINANCE_QUANTIZATION_TABLE[i];
        (*luminance)[i] = (int)(*luminance)[i];
    }
}

void Inverse_quantize(double** luminance){
    
    for(size_t i = 0; i< 64; i++){
        (*luminance)[i] *= LUMINANCE_QUANTIZATION_TABLE[i];
    }
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

    printf("Performing chroma subsampling on red chrominance...\n");
    chroma_subsample(&rChrominance_matrix, image);

    printf("Building blue chrominance matrix...\n");
    uint8_t** bChrominance_matrix;
    build_bChrominance_matrix(image, &bChrominance_matrix);

    printf("Performing chroma subsampling on blue chrominance...\n");
    chroma_subsample(&bChrominance_matrix, image);

    size_t total_blocks = (size_t)ceil((double)image.pixel_count / 64);
    printf("Dividing input into 8x8 blocks (Total blocks: %zu)...\n", total_blocks);
    PixelGroup* blocks = divide_image(luminance_matrix, rChrominance_matrix, bChrominance_matrix, image, 8);

    printf("First luminance block (8x8):\n");
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[100].lum_values[i * 8 + j]));
        }
        printf("\n");
    }



    printf("First red chrominance block (4x8):\n");
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[0].r_values[i * 8 + j]));
        }
        printf("\n");
    }



    printf("Computing DCT-II coefficients for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
       // printf("Processing block %zu/%zu\n", i + 1, total_blocks);
        discrete_cosine_transform(blocks[i].lum_values, 64, &(blocks[i].lum_coefficients));
        discrete_cosine_transform(blocks[i].r_values, 32, &(blocks[i].r_coefficients));
        discrete_cosine_transform(blocks[i].b_values, 32, &(blocks[i].b_coefficients));
    }

        printf("First luminance coefficients block (8x8):\n");
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[100].lum_coefficients[i * 8 + j]));
        }
        printf("\n");
    }



    printf("First red coefficients block (4x8):\n");
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[0].r_coefficients[i * 8 + j]));
        }
        printf("\n");
    }

    printf("Quantizing the matrices for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        //printf("Quantizing block %zu/%zu\n", i + 1, total_blocks);
        Quantize(&(blocks[i].lum_coefficients));
    }

    printf("Inverse-quantizing the matrices for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        //printf("Inverse quantizing block %zu/%zu\n", i + 1, total_blocks);
        Inverse_quantize(&(blocks[i].lum_coefficients));
    }

    printf("Performing Inverse-DCT on the values for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
       // printf("Inverse DCT on block %zu/%zu\n", i + 1, total_blocks);
        inverse_discrete_cosine_transform(&(blocks[i].lum_values), 64, blocks[i].lum_coefficients);
    }

    printf("Assembling the reconstructed image...\n");
    ImageData new_image = {0};
    assemble_image(&new_image, image, blocks);

    printf("Saving the reconstructed PNG image...\n");
    create_png_image("reconstructed.png", new_image.width, new_image.height, new_image.pixels);

    printf("Calculating Mean Squared Error (MSE)...\n");
    calculate_mse(luminance_matrix, new_image.pixels, new_image.height, new_image.width);

    printf("First luminance block after reconstruction (8x8):\n");
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[100].lum_values[i * 8 + j]));
        }
        printf("\n");
    }

    printf("Freeing allocated memory...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        free(blocks[i].lum_coefficients);
        free(blocks[i].b_coefficients);
        free(blocks[i].r_coefficients);
    }
    free(blocks);
    free_pixels(image.pixels, image.height);
    free_pixels(new_image.pixels, new_image.height);

    printf("Program completed successfully.\n");
    return 0;
}
