#include "bmp8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy

// 1.2.1 The bmp8_loadImage function
t_bmp8 *bmp8_loadImage(const char *filename) {
    FILE *file = fopen(filename, "rb"); // Open in binary read mode
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    // Allocate memory for the image structure
    t_bmp8 *img = (t_bmp8 *)malloc(sizeof(t_bmp8));
    if (!img) {
        fprintf(stderr, "Error allocating memory for image structure\n");
        fclose(file);
        return NULL;
    }

    // Read the BMP header (54 bytes)
    if (fread(img->header, sizeof(unsigned char), 54, file) != 54) {
        fprintf(stderr, "Error reading BMP header\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Check BMP signature ('BM')
    if (img->header[0] != 'B' || img->header[1] != 'M') {
        fprintf(stderr, "Error: Not a valid BMP file\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Extract image info using direct memory access with casting
    img->width = *(unsigned int *)&img->header[18];
    img->height = *(unsigned int *)&img->header[22];
    img->colorDepth = *(unsigned short *)&img->header[28]; // colorDepth is 2 bytes (unsigned short)
    img->dataSize = *(unsigned int *)&img->header[34];
    unsigned int dataOffset = *(unsigned int *)&img->header[10];

    // Validate color depth (must be 8 for this part)
    if (img->colorDepth != 8) {
        fprintf(stderr, "Error: Image color depth is not 8 bits (%d)\n", img->colorDepth);
        fclose(file);
        free(img);
        return NULL;
    }

    // Read the color table (1024 bytes for 8-bit)
    if (fread(img->colorTable, sizeof(unsigned char), 1024, file) != 1024) {
        fprintf(stderr, "Error reading color table\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // If dataSize is not provided in the header, calculate it
    // Note: This assumes no padding, as specified for barbara_gray.bmp
    if (img->dataSize == 0) {
        img->dataSize = img->width * img->height; // 1 byte per pixel for 8-bit
    }

    // Allocate memory for pixel data
    img->data = (unsigned char *)malloc(img->dataSize);
    if (!img->data) {
        fprintf(stderr, "Error allocating memory for pixel data\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Seek to the beginning of pixel data
    fseek(file, dataOffset, SEEK_SET);

    // Read pixel data
    if (fread(img->data, sizeof(unsigned char), img->dataSize, file) != img->dataSize) {
        fprintf(stderr, "Error reading pixel data\n");
        fclose(file);
        free(img->data);
        free(img);
        return NULL;
    }

    fclose(file);
    printf("Image '%s' loaded successfully.\n", filename);
    return img;
}

// 1.2.2 The bmp8_saveImage function
void bmp8_saveImage(const char *filename, t_bmp8 *img) {
    if (!img) {
        fprintf(stderr, "Error: Cannot save NULL image.\n");
        return;
    }

    FILE *file = fopen(filename, "wb"); // Open in binary write mode
    if (!file) {
        perror("Error opening file for writing");
        return;
    }

    // Write the BMP header (54 bytes)
    if (fwrite(img->header, sizeof(unsigned char), 54, file) != 54) {
        fprintf(stderr, "Error writing BMP header\n");
        fclose(file);
        return;
    }

    // Write the color table (1024 bytes)
    if (fwrite(img->colorTable, sizeof(unsigned char), 1024, file) != 1024) {
        fprintf(stderr, "Error writing color table\n");
        fclose(file);
        return;
    }

    // Write the pixel data
    if (fwrite(img->data, sizeof(unsigned char), img->dataSize, file) != img->dataSize) {
        fprintf(stderr, "Error writing pixel data\n");
        fclose(file);
        return;
    }

    fclose(file);
    printf("Image saved successfully to '%s'.\n", filename);
}

// 1.2.3 The bmp8_free function
void bmp8_free(t_bmp8 *img) {
    if (img) {
        if (img->data) {
            free(img->data);
            img->data = NULL; // Avoid double free
        }
        free(img);
    }
}

// 1.2.4 The bmp8_printInfo function
void bmp8_printInfo(t_bmp8 *img) {
    if (!img) {
        printf("Image Info: NULL image\n");
        return;
    }

    printf("Image Info:\n");
    printf("  Width: %u\n", img->width);
    printf("  Height: %u\n", img->height);
    printf("  Color Depth: %u\n", img->colorDepth);
    printf("  Data Size: %u bytes\n", img->dataSize);
    // Optional: Add more info if needed, e.g., header fields
    // unsigned int dataOffset = *(unsigned int *)&img->header[10];
    // printf("  Data Offset: %u\n", dataOffset);
}

// 1.3 Image Processing Functions

// 1.3.1 The bmp8_negative function
void bmp8_negative(t_bmp8 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply negative to NULL image or image with no data.\n");
        return;
    }
    for (unsigned int i = 0; i < img->dataSize; ++i) {
        img->data[i] = 255 - img->data[i];
    }
    printf("Negative filter applied.\n");
}

// 1.3.2 The bmp8_brightness function
void bmp8_brightness(t_bmp8 *img, int value) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply brightness to NULL image or image with no data.\n");
        return;
    }
    for (unsigned int i = 0; i < img->dataSize; ++i) {
        int newValue = (int)img->data[i] + value;
        // Clamp the value between 0 and 255
        if (newValue < 0) {
            img->data[i] = 0;
        } else if (newValue > 255) {
            img->data[i] = 255;
        } else {
            img->data[i] = (unsigned char)newValue;
        }
    }
    printf("Brightness adjusted by %d.\n", value);
}

// 1.3.3 The bmp8_threshold function
void bmp8_threshold(t_bmp8 *img, int threshold) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply threshold to NULL image or image with no data.\n");
        return;
    }
    if (threshold < 0 || threshold > 255) {
         fprintf(stderr, "Warning: Threshold value %d is outside the valid range [0, 255]. Clamping.\n", threshold);
         threshold = (threshold < 0) ? 0 : 255;
    }
    unsigned char threshold_val = (unsigned char)threshold;

    for (unsigned int i = 0; i < img->dataSize; ++i) {
        img->data[i] = (img->data[i] >= threshold_val) ? 255 : 0;
    }
    printf("Threshold filter applied with threshold %d.\n", threshold_val);
}

// Placeholder for other functions to avoid linker errors if main is compiled
// void bmp8_negative(t_bmp8 *img) { (void)img; /* TODO */ }
// void bmp8_brightness(t_bmp8 *img, int value) { (void)img; (void)value; /* TODO */ }
// void bmp8_threshold(t_bmp8 *img, int threshold) { (void)img; (void)threshold; /* TODO */ }

// 1.4 Image filtering

// Helper function to get pixel value safely (handles boundaries by returning 0)
// Note: This simple boundary handling might not be ideal for all filters.
// The spec says to ignore edges for now, which bmp8_applyFilter does.
// This helper isn't strictly needed for that approach but can be useful.
/*
static unsigned char getPixel(const t_bmp8 *img, int x, int y) {
    if (x < 0 || x >= (int)img->width || y < 0 || y >= (int)img->height) {
        return 0; // Treat out-of-bounds pixels as black
    }
    return img->data[y * img->width + x];
}
*/

// 1.4.1 The bmp8_applyFilter function
void bmp8_applyFilter(t_bmp8 *img, float **kernel, int kernelSize) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply filter to NULL image or image with no data.\n");
        return;
    }
    if (!kernel || kernelSize <= 0 || kernelSize % 2 == 0) {
        fprintf(stderr, "Error: Invalid kernel provided.\n");
        return;
    }

    unsigned int width = img->width;
    unsigned int height = img->height;
    unsigned int dataSize = img->dataSize;

    // Create a temporary buffer to store the original pixel data
    unsigned char *originalData = (unsigned char *)malloc(dataSize);
    if (!originalData) {
        fprintf(stderr, "Error: Could not allocate memory for filter buffer.\n");
        return;
    }
    memcpy(originalData, img->data, dataSize);

    int kernelRadius = kernelSize / 2;
    float sum;
    int pixelValue;

    // Iterate through the image pixels, skipping the borders
    // as specified ("We start with the pixels from (1, 1) and stop at (width - 2, height - 2)")
    // This means a kernelRadius of 1 (3x3 kernel) skips 1 pixel border.
    // For larger kernels, this border needs to be kernelRadius.
    for (unsigned int y = kernelRadius; y < height - kernelRadius; ++y) {
        for (unsigned int x = kernelRadius; x < width - kernelRadius; ++x) {
            sum = 0.0f;

            // Apply the kernel
            for (int ky = -kernelRadius; ky <= kernelRadius; ++ky) {
                for (int kx = -kernelRadius; kx <= kernelRadius; ++kx) {
                    // Get the pixel value from the *original* data
                    unsigned char p = originalData[(y + ky) * width + (x + kx)];
                    // Get the kernel value (adjust indices to be 0-based for array access)
                    float k = kernel[ky + kernelRadius][kx + kernelRadius];
                    sum += (float)p * k;
                }
            }

            // Clamp the result to [0, 255]
            pixelValue = (int)(sum + 0.5f); // Add 0.5 for rounding before truncating
            if (pixelValue < 0) pixelValue = 0;
            if (pixelValue > 255) pixelValue = 255;

            // Update the pixel in the image's data buffer
            img->data[y * width + x] = (unsigned char)pixelValue;
        }
    }

    // Free the temporary buffer
    free(originalData);
    printf("Filter applied (kernel size %d). Edges ignored.\n", kernelSize);
}

// Placeholder for other functions to avoid linker errors if main is compiled
// void bmp8_applyFilter(t_bmp8 *img, float **kernel, int kernelSize) { (void)img; (void)kernel; (void)kernelSize; /* TODO */ } 