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
            (*matrix)[y][x] = 0.5 * data.pixels[y][x].r - 0.42 * data.pixels[y][x].g - 0.081 * data.pixels[y][x].b;
            //printf("%d\n", (*matrix)[y][x]);
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
            (*matrix)[y][x] =  0.17 * data.pixels[y][x].r - 0.33 * data.pixels[y][x].g + 0.5 * data.pixels[y][x].b;
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
    for (size_t x = 0; x < width; x++) {          
        for (size_t y = 0; y < height; y++) {      
            double sum = 0.0;

           for (size_t u = 0; u < height; u++) {  
    for (size_t v = 0; v < width; v++) {
                    double alpha_u = (u == 0) ? sqrt(1.0 / width) : sqrt(2.0 / width);
                    double alpha_v = (v == 0) ? sqrt(1.0 / height) : sqrt(2.0 / height);

                    double cos_x = cos((PI * (2 * x + 1) * u) / (2.0 * width));
                    double cos_y = cos((PI * (2 * y + 1) * v) / (2.0 * height));

                    sum += alpha_u * alpha_v *coefficients[u * width + v] * cos_x * cos_y;
                }
            }

            // Store result in temp buffer
            temp_values[x * width + y] = sum;
        }
    }

    // Convert to uint8_t and clamp
    for (size_t i = 0; i < width * height; i++) {
        int value = (int)round(temp_values[i] + 128.0);
        values[i] = (value < 0) ? 0 : (value > 255) ? 255 : (uint8_t)value;
    }

    free(temp_values);
}







void discrete_cosine_transform(uint8_t* data, size_t data_width, size_t data_height, double** coefficients) {
    *coefficients = malloc(data_width * data_height * sizeof(double));
    int* corrected_values = malloc(data_width * data_height * sizeof(int));

    if (!*coefficients || !corrected_values) {
        free(*coefficients);
        free(corrected_values);
        fprintf(stderr, "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Shift input data range
    for (size_t i = 0; i < data_width * data_height; i++) {
        corrected_values[i] = (int)data[i] - 128;
    }

    // printf("Corrected values:\n");
    // for (size_t i = 0; i < data_width * data_height; i++) {
    //     printf("%d ", corrected_values[i]);
    //     if ((i + 1) % data_width == 0) printf("\n");
    // }
    // printf("\n");

    // Perform the 2D DCT-II
    for (size_t u = 0; u < data_height; u++) {  
        for (size_t v = 0; v < data_width; v++) {  
            double sum = 0.0;

            for (size_t x = 0; x < data_height; x++) { 
                for (size_t y = 0; y < data_width; y++) { 
                    double cos_x = cos((PI * (2 * x + 1) * u) / (2.0 * data_height));
                    double cos_y = cos((PI * (2 * y + 1) * v) / (2.0 * data_width));
                    sum += corrected_values[x * data_width + y] * cos_x * cos_y;
                }
            }

         //   printf("Sum at (u=%zu, v=%zu): %f\n", u, v, sum);

            double alpha_u = (u == 0) ? 1/sqrt(2) : 1;
            double alpha_v = (v == 0) ? 1/sqrt(2) : 1;
            (*coefficients)[u * data_width + v] = alpha_u * alpha_v * sum*(2/(sqrt(data_height*data_width)));

            //printf("Coefficient at (u=%zu, v=%zu): %f\n", u, v, (*coefficients)[u * data_width + v]);
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
                uint8_t Y = groups[block_index].lum_values[local_index];
uint8_t Cb = groups[block_index].b_values[local_index]; //doesn't account for chroma subsample
uint8_t Cr = groups[block_index].r_values[local_index]; 

// Convert YCbCr to RGB
int R = (int)(Y + 1.402 * (Cr - 128));
int G = (int)(Y - 0.344136 * (Cb - 128) - 0.714136 * (Cr - 128));
int B = (int)(Y + 1.772 * (Cb - 128));

// Clamp values to [0, 255]
R = (R < 0) ? 0 : (R > 255) ? 255 : R;
G = (G < 0) ? 0 : (G > 255) ? 255 : G;
B = (B < 0) ? 0 : (B > 255) ? 255 : B;

// Assign to output pixel
output->pixels[global_row][global_col].r = (uint8_t)R;
output->pixels[global_row][global_col].g = (uint8_t)G;
output->pixels[global_row][global_col].b = (uint8_t)B;
output->pixels[global_row][global_col].a = 255;
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

    printf("First luminance block (8x8):\n");
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (blocks[0].lum_values[i * 8 + j]));
        }
        printf("\n");
    }



    printf("First red chrominance block (4x8):\n");
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (blocks[0].r_values[i * 8 + j]));
        }
        printf("\n");
    }



    printf("Computing DCT-II coefficients for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
       // printf("Processing block %zu/%zu\n", i + 1, total_blocks);
        discrete_cosine_transform(blocks[i].lum_values, 8,8, &(blocks[i].lum_coefficients));
        discrete_cosine_transform(blocks[i].r_values, 4,8, &(blocks[i].r_coefficients));
        discrete_cosine_transform(blocks[i].b_values, 4,8, &(blocks[i].b_coefficients));
    }

        printf("First luminance coefficients block (8x8):\n");
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
           printf("%.2f ", blocks[0].lum_coefficients[i * 8 + j]);
        }
        printf("\n");
    }



    printf("Quantizing the matrices for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        //printf("Quantizing block %zu/%zu\n", i + 1, total_blocks);
       Quantize(&(blocks[i].lum_coefficients), LUMINANCE_QUANTIZATION_TABLE, 64);
       Quantize(&(blocks[i].b_coefficients), CHROMINANCE_QUANTIZATION_TABLE, 32);
       Quantize(&(blocks[i].r_coefficients),CHROMINANCE_QUANTIZATION_TABLE, 32);
    }

            printf("First luminance coefficients quantized block (8x8):\n");
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[0].lum_coefficients[i * 8 + j]));
        }
        printf("\n");
    }

    printf("Inverse-quantizing the matrices for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
        //printf("Inverse quantizing block %zu/%zu\n", i + 1, total_blocks);
       Inverse_quantize(&(blocks[i].lum_coefficients), LUMINANCE_QUANTIZATION_TABLE, 64);
           Inverse_quantize(&(blocks[i].b_coefficients), CHROMINANCE_QUANTIZATION_TABLE, 32);
       Inverse_quantize(&(blocks[i].r_coefficients),CHROMINANCE_QUANTIZATION_TABLE, 32);
    }

    printf("Performing Inverse-DCT on the values for all blocks...\n");
    for (size_t i = 0; i < total_blocks; i++) {
       // printf("Inverse DCT on block %zu/%zu\n", i + 1, total_blocks);
       inverse_discrete_cosine_transform(blocks[i].lum_values, 8,8, blocks[i].lum_coefficients);
        inverse_discrete_cosine_transform(blocks[i].b_values, 4,8, blocks[i].b_coefficients);
        inverse_discrete_cosine_transform(blocks[i].r_values, 4,8, blocks[i].r_coefficients);
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
            printf("%d ", (int)(blocks[0].lum_values[i * 8 + j]));
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
