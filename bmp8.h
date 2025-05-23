#ifndef BMP8_H
#define BMP8_H

#include <stdio.h> // For FILE type in function prototypes if needed later
#include <stdlib.h> // For size_t

// Define the structure for an 8-bit BMP image
typedef struct {
    unsigned char header[54];      // BMP file header (54 bytes)
    unsigned char colorTable[1024]; // Color table (for 8-bit grayscale)
    unsigned char *data;           // Pixel data
    unsigned int width;            // Image width in pixels
    unsigned int height;           // Image height in pixels
    unsigned int colorDepth;       // Color depth (should be 8)
    unsigned int dataSize;         // Size of pixel data in bytes
} t_bmp8;

// --- Function Prototypes for Part 1 ---

// 1.2 Reading and Writing
t_bmp8 *bmp8_loadImage(const char *filename);
void bmp8_saveImage(const char *filename, t_bmp8 *img);
void bmp8_free(t_bmp8 *img);
void bmp8_printInfo(t_bmp8 *img);

// 1.3 Image Processing Functions
void bmp8_negative(t_bmp8 *img);
void bmp8_brightness(t_bmp8 *img, int value);
void bmp8_threshold(t_bmp8 *img, int threshold);

// 1.4 Image filtering
void bmp8_applyFilter(t_bmp8 *img, float **kernel, int kernelSize);

#endif // BMP8_H 