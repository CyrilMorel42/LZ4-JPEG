#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include "stb_image.h"

#define IMAGES_DIRECTORY "../../../Assets/Images/"

typedef struct{
    unsigned char r, g, b, a;
} Pixel;

typedef struct {
    Pixel **pixels;
    int height;
    int width;
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

    stbi_image_free(data);
    return result;
}

char* construct_path(char* image_name, char* directory){
    char* path = malloc(sizeof(image_name)+sizeof(directory)+1);
    strcpy(path, directory);
    strcat(path, image_name);
    return path;
}

int main() {
    ImageData image = read_image(construct_path("og.png", IMAGES_DIRECTORY));

    printf("Image dimensions: %d x %d\n", image.width, image.height);
    printf("Pixel (0, 0) -> R: %u, G: %u, B: %u, A: %u\n",
           image.pixels[0][0].r, image.pixels[0][0].g, image.pixels[0][0].b, image.pixels[0][0].a);

    free_pixels(image.pixels, image.height);

    return 0;
}
