
#define STB_IMAGE_WRITE_IMPLEMENTATION


#include <stdio.h>
#include <stdlib.h>
#include "stb_image_write.h"
char* OUT_DIRECTORY = "../Assets/Images/";

typedef struct {
    unsigned char r, g, b, a;
} Pixel;

typedef struct {
    Pixel **pixels;
    int height;
    int width;
    size_t pixel_count;
} ImageData;


char* construct_path(char* image_name, char* directory) {
    char* path = malloc(strlen(image_name) + strlen(directory) + 1);
    strcpy(path, directory);
    strcat(path, image_name);
    return path;
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
    char* path = construct_path(filename, OUT_DIRECTORY);

    if (stbi_write_png(path, width, height, 4, rgb_image, width * 4) == 0) {
        printf("Error: Failed to write the PNG image.\n");
    }


    free(rgb_image);
    free(path);
}

void generate_noise_image(ImageData* data){
     (*data).pixels = malloc((*data).height * sizeof(Pixel *));
    for (int y = 0; y <(*data). height; y++) {
        (*data).pixels[y] = malloc((*data).width * sizeof(Pixel));
    }

    for (int y = 0; y <(*data).height; y++) {
        for (int x = 0; x < (*data).width; x++) {
            Pixel pixel;
            pixel.a=255;
            pixel.r = rand() % 256;
            pixel.g = rand() % 256;
            pixel.b = rand() % 256;

              (*data).pixels[y][x] = pixel;
        }
    }

    create_png_image("rand_8X8.png", data->width, data->height, data->pixels);
}


void free_pixels(Pixel** pixels, int height) {
    for (int y = 0; y < height; y++) {
        free(pixels[y]);
    }
    free(pixels);
}

