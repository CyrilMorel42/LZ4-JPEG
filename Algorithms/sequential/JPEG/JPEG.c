#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include "stb_image.h"
#include "stb_image_write.h"

#define IMAGES_DIRECTORY "../../../Assets/Images/"
#define OUTPUT_DIRECTORY "../../../Output-Input/Images/"

typedef struct {
    unsigned char r, g, b, a;
} Pixel;

typedef struct {
    Pixel **pixels;
    int height;
    int width;
    size_t pixel_count;
} ImageData;

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

    // Allocate memory for pixels
    Pixel **pixels = malloc(height * sizeof(Pixel *));
    for (int y = 0; y < height; y++) {
        pixels[y] = malloc(width * sizeof(Pixel));
    }

    // Map image data to pixels
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_index = (y * width + x) * channels;
            pixels[y][x].r = data[pixel_index];                  // Red channel
            pixels[y][x].g = data[pixel_index + 1];              // Green channel
            pixels[y][x].b = data[pixel_index + 2];              // Blue channel
            pixels[y][x].a = (channels == 4) ? data[pixel_index + 3] : 255;  // Alpha channel (defaults to 255 if not present)
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
    // Allocate memory for luminance matrix
    *matrix = malloc(data.height * sizeof(uint8_t*));
    for (int y = 0; y < data.height; y++) {
        (*matrix)[y] = malloc(data.width * sizeof(uint8_t));
    }

    // Calculate luminance values
    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            (*matrix)[y][x] = 0.299 * data.pixels[y][x].r + 0.587 * data.pixels[y][x].g + 0.114 * data.pixels[y][x].b;
        }
    }
}

void build_rChrominance_matrix(ImageData data, uint8_t*** matrix) {
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
    // Allocate memory for a flat array of RGBA values
    unsigned char* rgb_image = malloc(width * height * 4);  // 4 channels per pixel (RGBA)

    if (rgb_image == NULL) {
        printf("Error: Unable to allocate memory for the image.\n");
        return;
    }

    // Flatten the pixel data into the rgb_image array
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t index = (y * width + x) * 4;  // 4 channels (RGBA)
            rgb_image[index + 0] = pixels[y][x].r;  // Red channel
            rgb_image[index + 1] = pixels[y][x].g;  // Green channel
            rgb_image[index + 2] = pixels[y][x].b;  // Blue channel
            rgb_image[index + 3] = pixels[y][x].a;  // Alpha channel
        }
    }
    char* path = construct_path(filename, OUTPUT_DIRECTORY);
    // Write the image to a PNG file (RGB image, 4 channels)
    if (stbi_write_png(path, width, height, 4, rgb_image, width * 4) == 0) {
        printf("Error: Failed to write the PNG image.\n");
    }

    // Free the flattened image data
    free(rgb_image);
    free(path);
}

void create_luminance_image(char* filename,uint8_t** values, ImageData data) {
    Pixel **pixels = malloc(data.height * sizeof(Pixel *));
    for (int y = 0; y < data.height; y++) {
        pixels[y] = malloc(data.width * sizeof(Pixel));
    }

    // Assign luminance values to each pixel
    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            pixels[y][x].r = values[y][x];
            pixels[y][x].g = values[y][x];
            pixels[y][x].b = values[y][x];
            pixels[y][x].a = 255;  // Assign luminance value to alpha as well
        }
    }

    // Create PNG image from luminance data
    create_png_image(filename, data.width, data.height, pixels);

    // Free the pixel data
    free_pixels(pixels, data.height);
}

void print_value(uint8_t** values, ImageData data){
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

    // Assign luminance values to each pixel
    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
            pixels[y][x].r = values[y][x];
            pixels[y][x].g = 0;
            pixels[y][x].b = 0;
            pixels[y][x].a = 255;  // Assign luminance value to alpha as well
        }
    }

    // Create PNG image from luminance data
    create_png_image(filename, data.width, data.height, pixels);

    // Free the pixel data
    free_pixels(pixels, data.height);
}

void create_bChrominance_image(char* filename,uint8_t** values, ImageData data) {
    Pixel **pixels = malloc(data.height * sizeof(Pixel *));
    for (int y = 0; y < data.height; y++) {
        pixels[y] = malloc(data.width * sizeof(Pixel));
    }

    // Assign luminance values to each pixel
    

    // Create PNG image from luminance data
    create_png_image(filename, data.width, data.height, pixels);

    // Free the pixel data
    free_pixels(pixels, data.height);
}

void chroma_subsample(uint8_t*** values, ImageData data){ //technically, this would reduce the original size of the matrix, however, it is very complex to deal with, so for this research's purposes, i will only implement the effects on the image, not the transmitting part
    for (size_t y = 0; y < data.height; y++) {
        for (size_t x = 0; x < data.width; x++) {
              if(x % 2==1){
                (*values)[y][x] = (*values)[y][x-1];
              }
        }
    }
}

int main() {
    char* path = construct_path("og.png", IMAGES_DIRECTORY);
    ImageData image = read_image(path);
    free(path);
    // Create PNG of the original image
    create_png_image("original.png", image.width, image.height, image.pixels);

    // Create the luminance matrix
    uint8_t** luminance_matrix;
    build_luminance_matrix(image, &luminance_matrix);


    // Create PNG image of luminance
    create_luminance_image("luminance.png",luminance_matrix, image);

    // Create the luminance matrix
    uint8_t** rChrominance_matrix;
    build_rChrominance_matrix(image, &rChrominance_matrix);
    chroma_subsample(&rChrominance_matrix, image);



    // Create PNG image of luminance
    create_rChrominance_image("rChrominance.png",rChrominance_matrix, image);

    uint8_t** bChrominance_matrix;
    build_bChrominance_matrix(image, &bChrominance_matrix);
    chroma_subsample(&bChrominance_matrix, image);

    // Create PNG image of luminance
    create_rChrominance_image("bChrominance.png",bChrominance_matrix, image);

    // Free memory
    free_pixels(image.pixels, image.height);
    for (int y = 0; y < image.height; y++) {
        free(luminance_matrix[y]);
    }
    free(luminance_matrix);

    return 0;
}
