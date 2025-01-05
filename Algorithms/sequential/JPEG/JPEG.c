#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include "stb_image.h"
#include "stb_image_write.h"


#define IMAGES_DIRECTORY "../../../Assets/Images/"
#define OUTPUT_DIRECTORY "../../../Output-Input/Images/"
#define PI 3.14159265358979323846
size_t LUMINANCE_QUANTIZATION_TABLE [64] = {
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
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
    uint8_t values[64];
    size_t index;
    double* coefficients;
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

PixelGroup* divide_image(uint8_t** values, ImageData data, size_t group_size) {

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
            groups[i].values[j] = 0; 
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

            groups[block_index].values[local_index] = values[row][col];
        }
    }



    return groups;
}

void assemble_image(ImageData* output, ImageData original, PixelGroup* groups){
    //stitch together the blocks
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
    char* path = construct_path("og.png", IMAGES_DIRECTORY);
    ImageData image = read_image(path);
    free(path);

    create_png_image("original.png", image.width, image.height, image.pixels);


    uint8_t** luminance_matrix;
    build_luminance_matrix(image, &luminance_matrix);
    uint8_t** rChrominance_matrix;
    build_rChrominance_matrix(image, &rChrominance_matrix);
    chroma_subsample(&rChrominance_matrix, image);


    uint8_t** bChrominance_matrix;
    build_bChrominance_matrix(image, &bChrominance_matrix);
    chroma_subsample(&bChrominance_matrix, image);



     size_t total_blocks = (size_t)ceil((double)image.pixel_count / 64);
         printf("dividing input into 8x8 blocks...\n");
     PixelGroup* blocks = divide_image(luminance_matrix, image, 8);
      printf("First group (8x8 block):\n");
    for (size_t i = 0; i < 8; i++) { 
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[0].values[i * 8 + j])); 
        }
        printf("\n");
    }
     printf("Computing DCT-II coefficients...\n");
     for(size_t i = 0; i < total_blocks; i++){
        
         discrete_cosine_transform(blocks[i].values, 64, &(blocks[i].coefficients));;
     }
     printf("Quantizing the matrices...\n");
     for(size_t i = 0; i < total_blocks; i++){
        
         Quantize(&(blocks[i].coefficients));

     }
     printf("Inverse-quantizing the matrices...\n");
     for(size_t i = 0; i < total_blocks; i++){
         Inverse_quantize(&(blocks[i].coefficients));
     }

         printf("Inverse-DCT the values...\n");
     for(size_t i = 0; i < total_blocks; i++){
         inverse_discrete_cosine_transform(&(blocks[i].values), 64, blocks[i].coefficients);
     }

 printf("First group (8x8 block):\n");
    for (size_t i = 0; i < 8; i++) { 
        for (size_t j = 0; j < 8; j++) {
            printf("%d ", (int)(blocks[0].values[i * 8 + j])); 
        }
        printf("\n");
    }

    
    for(size_t i = 0; i < total_blocks; i++){
         free(blocks[i].coefficients);
    }
     free(blocks);
    free_pixels(image.pixels, image.height);
    

    return 0;
}
