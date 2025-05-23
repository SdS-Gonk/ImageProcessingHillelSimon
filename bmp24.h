#ifndef BMP24_H
#define BMP24_H

#include <stdint.h> // For fixed-width integer types (uint16_t, uint32_t, etc.)
#include <stdio.h>  // For FILE type

// --- BMP File Structures (Part 2.2.1) ---

// Ensure structures are packed tightly to match file format
#pragma pack(push, 1)
typedef struct {
    uint16_t type;      // Signature: 'BM' (0x4D42)
    uint32_t size;      // File size in bytes
    uint16_t reserved1; // Reserved, must be 0
    uint16_t reserved2; // Reserved, must be 0
    uint32_t offset;    // Offset to start of pixel data
} t_bmp_header;

typedef struct {
    uint32_t size;            // Header size in bytes (40)
    int32_t  width;           // Image width in pixels
    int32_t  height;          // Image height in pixels
    uint16_t planes;          // Number of color planes (must be 1)
    uint16_t bits;            // Bits per pixel (1, 4, 8, 16, 24, 32)
    uint32_t compression;     // Compression type (0 = uncompressed)
    uint32_t imagesize;       // Image size in bytes (can be 0 for uncompressed)
    int32_t  xresolution;     // Horizontal resolution (pixels per meter)
    int32_t  yresolution;     // Vertical resolution (pixels per meter)
    uint32_t ncolors;         // Number of colors in palette (0 for 24-bit)
    uint32_t importantcolors; // Important colors (usually 0)
} t_bmp_info;
#pragma pack(pop)

// --- Pixel Structure (Part 2.2.2) ---
typedef struct {
    uint8_t blue;  // Blue component (BMP stores in BGR order)
    uint8_t green; // Green component
    uint8_t red;   // Red component
} t_pixel; // Note: This is 3 bytes

// --- 24-bit BMP Image Structure (Part 2.2) ---
typedef struct {
    t_bmp_header header;      // File header
    t_bmp_info header_info; // Image information header
    int width;            // Image width (extracted for convenience)
    int height;           // Image height (extracted for convenience)
    int colorDepth;       // Color depth (should be 24)
    t_pixel **data;       // Pixel data (2D array of t_pixel)
} t_bmp24;

// --- Useful Constants (Part 2.2.3) ---
// Offsets (using struct members is generally safer, but these are for reference)
#define BITMAP_MAGIC_OFFSET   0x00
#define BITMAP_SIZE_OFFSET    0x02
#define BITMAP_OFFSET_OFFSET  0x0A
#define BITMAP_WIDTH_OFFSET   0x12 // Relative to start of t_bmp_info
#define BITMAP_HEIGHT_OFFSET  0x16 // Relative to start of t_bmp_info
#define BITMAP_DEPTH_OFFSET   0x1C // Relative to start of t_bmp_info
#define BITMAP_SIZE_RAW_OFFSET 0x22 // Relative to start of t_bmp_info

// Magical number for BMP files
#define BMP_TYPE 0x4D42 // 'BM'

// Header sizes
#define BMP_HEADER_SIZE sizeof(t_bmp_header) // Should be 14
#define BMP_INFO_SIZE sizeof(t_bmp_info)     // Should be 40

// Constant for the color depth
#define DEFAULT_COLOR_DEPTH 24

// --- Function Prototypes for Part 2 ---

// 2.3 Allocation and deallocation functions
t_pixel **bmp24_allocateDataPixels(int width, int height);
void bmp24_freeDataPixels(t_pixel **pixels, int height);
t_bmp24 *bmp24_allocate(int width, int height);
void bmp24_free(t_bmp24 *img);

// Helper functions for reading/writing raw data (Part 2.4.1)
// (Can be placed in utils.c or bmp24.c)
void file_rawRead(uint32_t position, void *buffer, uint32_t size, size_t n, FILE *file);
void file_rawWrite(uint32_t position, const void *buffer, uint32_t size, size_t n, FILE *file);

// Helper functions for reading/writing pixel data (Part 2.4.2)
// (These might be static helpers within bmp24.c)
// void bmp24_readPixelValue(t_bmp24 *image, int x, int y, FILE *file); // Maybe not needed directly
void bmp24_readPixelData(t_bmp24 *image, FILE *file);
// void bmp24_writePixelValue(t_bmp24 *image, int x, int y, FILE *file); // Maybe not needed directly
void bmp24_writePixelData(t_bmp24 *image, FILE *file);

// 2.4 Features: Loading and Saving 24-bit Images
t_bmp24 *bmp24_loadImage(const char *filename);
void bmp24_saveImage(t_bmp24 *img, const char *filename);

// 2.5 Features: 24-bit Image Processing
void bmp24_negative(t_bmp24 *img);
void bmp24_grayscale(t_bmp24 *img);
void bmp24_brightness(t_bmp24 *img, int value);

// 2.6 Features: Convolution Filters
t_pixel bmp24_convolution(t_bmp24 *img, int x, int y, float **kernel, int kernelSize, t_pixel **original_data);
void bmp24_applyFilter(t_bmp24 * img, float ** kernel, int kernelSize);
// Specific filter functions (can call bmp24_applyFilter)
void bmp24_boxBlur(t_bmp24 *img);
void bmp24_gaussianBlur(t_bmp24 *img);
void bmp24_outline(t_bmp24 *img);
void bmp24_emboss(t_bmp24 *img);
void bmp24_sharpen(t_bmp24 *img);

#endif // BMP24_H 