#include "bmp24.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy, memset
#include <math.h> // For abs() used in padding calculation

// --- Helper functions for reading/writing raw data (Part 2.4.1) ---

/*
* @brief Set the file cursor to the position position in the file file,
* then read n elements of size size from the file into the buffer.
*/
void file_rawRead(uint32_t position, void *buffer, uint32_t size, size_t n, FILE *file) {
    if (fseek(file, position, SEEK_SET) != 0) {
        perror("Error seeking file for read");
        // Handle error appropriately, maybe exit or return an error code
        // For simplicity, we might just let fread fail below.
    }
    if (fread(buffer, size, n, file) != n) {
        if (feof(file)) {
            fprintf(stderr, "Error reading raw data: Unexpected end of file at position %u\n", position);
        } else if (ferror(file)) {
            perror("Error reading raw data from file");
        } else {
            fprintf(stderr, "Error reading raw data: Unknown read error at position %u\n", position);
        }
        // Consider more robust error handling here
    }
}

/*
* @brief Set the file cursor to the position position in the file file,
* then write n elements of size size from the buffer into the file.
*/
void file_rawWrite(uint32_t position, const void *buffer, uint32_t size, size_t n, FILE *file) {
    if (fseek(file, position, SEEK_SET) != 0) {
        perror("Error seeking file for write");
        return; // Or handle error more robustly
    }
    if (fwrite(buffer, size, n, file) != n) {
        perror("Error writing raw data to file");
        // Consider more robust error handling here
    }
}

// --- Allocation and deallocation functions (Part 2.3) ---

/*
 * Dynamically allocate memory for a t_pixel matrix (2D array) of size width * height.
 * Returns the address allocated in the heap.
 * Returns NULL if allocation fails.
 */
t_pixel **bmp24_allocateDataPixels(int width, int height) {
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid dimensions (%d x %d) for pixel allocation.\n", width, height);
        return NULL;
    }

    // Allocate memory for the row pointers
    t_pixel **pixels = (t_pixel **)malloc(height * sizeof(t_pixel *));
    if (!pixels) {
        perror("Error allocating memory for pixel row pointers");
        return NULL;
    }

    // Allocate memory for the actual pixel data (contiguous block)
    // Note: Access will be pixels[y][x]
    pixels[0] = (t_pixel *)malloc(width * height * sizeof(t_pixel));
    if (!pixels[0]) {
        perror("Error allocating memory for pixel data block");
        free(pixels);
        return NULL;
    }

    // Set up the row pointers to point into the contiguous block
    for (int i = 1; i < height; ++i) {
        pixels[i] = pixels[i - 1] + width;
    }

    // Optional: Initialize pixels to black (or some default)
    // memset(pixels[0], 0, width * height * sizeof(t_pixel));

    return pixels;
}

/*
 * Free all the memory allocated for the t_pixel matrix pixels.
 */
void bmp24_freeDataPixels(t_pixel **pixels, int height) {
    if (!pixels) {
        return;
    }
    if (pixels[0]) { // Free the contiguous block
        free(pixels[0]);
    }
    free(pixels); // Free the row pointers
    (void)height; // Height parameter might be useful if allocation wasn't contiguous
}

/*
 * Dynamically allocate memory for a t_bmp24 image structure.
 * Calls bmp24_allocateDataPixels to allocate the data matrix.
 * Initializes width, height, and colorDepth fields.
 * Returns the address of the allocated image or NULL on failure.
 */
t_bmp24 *bmp24_allocate(int width, int height) {
    t_bmp24 *img = (t_bmp24 *)malloc(sizeof(t_bmp24));
    if (!img) {
        perror("Error allocating memory for t_bmp24 structure");
        return NULL;
    }

    // Initialize basic fields
    img->width = width;
    img->height = height;
    img->colorDepth = DEFAULT_COLOR_DEPTH; // Assume 24-bit for this structure
    img->data = NULL; // Initialize data pointer

    // Allocate the pixel data matrix
    img->data = bmp24_allocateDataPixels(width, height);
    if (!img->data) {
        fprintf(stderr, "Error allocating pixel data within bmp24_allocate.\n");
        free(img); // Free the image structure itself
        return NULL;
    }

    // Initialize header fields to sensible defaults (can be overwritten during load/save)
    memset(&img->header, 0, sizeof(t_bmp_header));
    memset(&img->header_info, 0, sizeof(t_bmp_info));

    // Set essential header fields that can be derived
    img->header.type = BMP_TYPE;
    img->header.offset = BMP_HEADER_SIZE + BMP_INFO_SIZE; // Offset to pixel data (no palette)

    img->header_info.size = BMP_INFO_SIZE;
    img->header_info.width = width;
    img->header_info.height = height;
    img->header_info.planes = 1;
    img->header_info.bits = DEFAULT_COLOR_DEPTH;
    img->header_info.compression = 0; // Uncompressed
    // Calculate row size including potential padding
    // Each row must be a multiple of 4 bytes
    uint32_t row_size = ((width * sizeof(t_pixel) * 8 + 31) / 32) * 4;
    img->header_info.imagesize = row_size * abs(height); // Total size of pixel data including padding
    img->header.size = img->header.offset + img->header_info.imagesize; // Total file size

    // Resolution fields can be set to zero or a default value
    img->header_info.xresolution = 0; // Example: 2835; // 72 DPI
    img->header_info.yresolution = 0; // Example: 2835; // 72 DPI
    img->header_info.ncolors = 0;
    img->header_info.importantcolors = 0;

    return img;
}

/*
 * Free all the memory allocated for the image img received as a parameter.
 */
void bmp24_free(t_bmp24 *img) {
    if (img) {
        if (img->data) {
            bmp24_freeDataPixels(img->data, img->height);
            img->data = NULL;
        }
        free(img);
    }
}

// --- Helper functions for reading/writing pixel data (Part 2.4.2) ---

/*
 * Reads the pixel data from the BMP file into the image->data matrix.
 * Handles bottom-up row order and padding.
 */
void bmp24_readPixelData(t_bmp24 *image, FILE *file) {
    if (!image || !image->data || !file) {
        fprintf(stderr, "Error: Invalid arguments for bmp24_readPixelData\n");
        return;
    }

    int width = image->width;
    int height = image->height;
    uint32_t offset = image->header.offset;

    // Calculate padding per row
    // Row size in bytes = width * bytes_per_pixel (3 for 24-bit)
    // Padding = (4 - (row_size % 4)) % 4;
    int bytes_per_pixel = image->header_info.bits / 8;
    if (bytes_per_pixel != 3) {
        fprintf(stderr, "Warning/Error: Expected 3 bytes per pixel for 24-bit, got %d. Reading might be incorrect.\n", bytes_per_pixel);
        // Decide how to handle: maybe return, maybe proceed cautiously.
        // For now, proceed assuming 3 BPP was intended based on t_pixel struct.
        bytes_per_pixel = 3;
    }
    uint32_t row_size_unpadded = width * bytes_per_pixel;
    uint32_t padding = (4 - (row_size_unpadded % 4)) % 4;
    uint32_t row_size_padded = row_size_unpadded + padding;

    // Seek to the start of pixel data
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("Error seeking to pixel data for read");
        return;
    }

    // Allocate a temporary buffer for one padded row
    unsigned char *row_buffer = (unsigned char *)malloc(row_size_padded);
    if (!row_buffer) {
        perror("Error allocating memory for row buffer");
        return;
    }

    // Read rows from bottom to top (as stored in file)
    // and store them from top to bottom in our data structure
    for (int y = height - 1; y >= 0; --y) {
        if (fread(row_buffer, sizeof(unsigned char), row_size_padded, file) != row_size_padded) {
            if (feof(file)) {
                fprintf(stderr, "Error reading pixel data row %d: Unexpected end of file\n", y);
            } else {
                 perror("Error reading pixel data row");
            }
            free(row_buffer);
            return;
        }

        // Copy pixels from the buffer to the image->data matrix
        // Note the BGR order in the file vs our t_pixel struct (which we also defined as BGR)
        // If t_pixel was RGB, we'd swap here.
        memcpy(image->data[y], row_buffer, row_size_unpadded);
    }

    free(row_buffer);
    // printf("Pixel data read successfully.\n"); // Optional success message
}

/*
 * Writes the pixel data from the image->data matrix to the BMP file.
 * Handles bottom-up row order and padding.
 */
void bmp24_writePixelData(t_bmp24 *image, FILE *file) {
     if (!image || !image->data || !file) {
        fprintf(stderr, "Error: Invalid arguments for bmp24_writePixelData\n");
        return;
    }

    int width = image->width;
    int height = image->height;
    uint32_t offset = image->header.offset;

    // Calculate padding per row
    int bytes_per_pixel = 3; // Assuming 24-bit
    uint32_t row_size_unpadded = width * bytes_per_pixel;
    uint32_t padding = (4 - (row_size_unpadded % 4)) % 4;
    uint32_t row_size_padded = row_size_unpadded + padding;

    // Seek to the start of pixel data location (though fwrite might handle this if file is sequential)
    // It's safer to seek explicitly.
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("Error seeking to pixel data for write");
        return;
    }

    // Allocate a temporary buffer for one padded row, initialized to zero
    unsigned char *row_buffer = (unsigned char *)calloc(row_size_padded, sizeof(unsigned char));
    if (!row_buffer) {
        perror("Error allocating memory for row buffer");
        return;
    }

    // Write rows from bottom to top (as required by BMP)
    // reading them from top to bottom from our data structure
    for (int y = height - 1; y >= 0; --y) {
        // Copy the actual pixel data to the start of the buffer
        memcpy(row_buffer, image->data[y], row_size_unpadded);
        // Padding bytes are already zero due to calloc

        // Write the padded row to the file
        if (fwrite(row_buffer, sizeof(unsigned char), row_size_padded, file) != row_size_padded) {
            perror("Error writing pixel data row");
            free(row_buffer);
            return;
        }
    }

    free(row_buffer);
    // printf("Pixel data written successfully.\n"); // Optional success message
}

// --- Features: Loading and Saving 24-bit Images (Part 2.4) ---

/*
 * Reads a BMP 24-bit image file and loads it into a t_bmp24 structure.
 */
t_bmp24 *bmp24_loadImage(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file for reading");
        return NULL;
    }

    // Read headers first to get dimensions and check validity
    t_bmp_header header;
    t_bmp_info header_info;

    // Read the first header
    file_rawRead(0, &header, sizeof(t_bmp_header), 1, file);
    if (header.type != BMP_TYPE) {
        fprintf(stderr, "Error: File '%s' is not a BMP file (invalid signature 0x%X).\n", filename, header.type);
        fclose(file);
        return NULL;
    }

    // Read the second header (info header)
    file_rawRead(BMP_HEADER_SIZE, &header_info, sizeof(t_bmp_info), 1, file);
    if (header_info.size != BMP_INFO_SIZE) {
        fprintf(stderr, "Warning: Unexpected DIB header size (%u, expected %lu) in '%s'.\n", header_info.size, (unsigned long)BMP_INFO_SIZE, filename);
        // Allow continuing, but it might indicate an unsupported BMP variant.
    }
    if (header_info.bits != DEFAULT_COLOR_DEPTH) {
        fprintf(stderr, "Error: Image '%s' is not a 24-bit BMP (%d bits).\n", filename, header_info.bits);
        fclose(file);
        return NULL;
    }
    if (header_info.compression != 0) {
        fprintf(stderr, "Error: Compressed BMP files are not supported (compression type %u).\n", header_info.compression);
        fclose(file);
        return NULL;
    }

    int width = header_info.width;
    // BMP height can be negative to indicate top-down row order, but we handle bottom-up reading anyway.
    // Use absolute height for allocation.
    int height = abs(header_info.height);
    if (width <= 0 || height <= 0) {
         fprintf(stderr, "Error: Invalid image dimensions (%d x %d) in '%s'.\n", width, height, filename);
         fclose(file);
         return NULL;
    }

    // Allocate the image structure and pixel data
    t_bmp24 *img = bmp24_allocate(width, height);
    if (!img) {
        // bmp24_allocate prints its own error
        fclose(file);
        return NULL;
    }

    // Copy the read headers into the allocated structure
    img->header = header;
    img->header_info = header_info;
    // Ensure internal width/height match, although bmp24_allocate already set them
    img->width = width;
    img->height = height;
    img->colorDepth = header_info.bits;

    // Read the actual pixel data using the dedicated function
    bmp24_readPixelData(img, file);
    // TODO: Check for errors from bmp24_readPixelData if it were modified to return status

    fclose(file);
    printf("24-bit image '%s' loaded successfully (%dx%d).\n", filename, width, height);
    return img;
}

/*
 * Saves a t_bmp24 image into a file in BMP 24-bit format.
 */
void bmp24_saveImage(t_bmp24 *img, const char *filename) {
    if (!img) {
        fprintf(stderr, "Error: Cannot save NULL image.\n");
        return;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file for writing");
        return;
    }

    // Ensure headers in the struct are consistent before writing
    // (bmp24_allocate should have set defaults, but modifications might have occurred)
    // Recalculate sizes just in case
    img->header.type = BMP_TYPE;
    img->header_info.size = BMP_INFO_SIZE;
    img->header_info.width = img->width;
    img->header_info.height = img->height; // Keep original sign if needed? Standard is usually positive.
    img->header_info.planes = 1;
    img->header_info.bits = DEFAULT_COLOR_DEPTH;
    img->header_info.compression = 0;
    uint32_t row_size = ((img->width * sizeof(t_pixel) * 8 + 31) / 32) * 4;
    img->header_info.imagesize = row_size * abs(img->height);
    img->header.offset = BMP_HEADER_SIZE + BMP_INFO_SIZE;
    img->header.size = img->header.offset + img->header_info.imagesize;
    img->header.reserved1 = 0;
    img->header.reserved2 = 0;
    img->header_info.xresolution = 0; // Or a default DPI value
    img->header_info.yresolution = 0;
    img->header_info.ncolors = 0;
    img->header_info.importantcolors = 0;

    // Write the headers
    file_rawWrite(0, &img->header, sizeof(t_bmp_header), 1, file);
    file_rawWrite(BMP_HEADER_SIZE, &img->header_info, sizeof(t_bmp_info), 1, file);

    // Write pixel data using the dedicated function
    bmp24_writePixelData(img, file);
    // TODO: Check for errors if bmp24_writePixelData were modified

    fclose(file);
    printf("Image saved successfully to '%s'.\n", filename);
}

// Helper function to clamp value between 0 and 255
static uint8_t clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

// --- Features: 24-bit Image Processing (Part 2.5) ---

/*
 * Invert the colors of the image (negative).
 */
void bmp24_negative(t_bmp24 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply negative to NULL image or image with no data.\n");
        return;
    }
    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            t_pixel *p = &img->data[y][x];
            p->red = 255 - p->red;
            p->green = 255 - p->green;
            p->blue = 255 - p->blue;
        }
    }
    printf("Negative filter applied.\n");
}

/*
 * Convert the image to grayscale.
 */
void bmp24_grayscale(t_bmp24 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply grayscale to NULL image or image with no data.\n");
        return;
    }
    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            t_pixel *p = &img->data[y][x];
            // Calculate average: (R + G + B) / 3
            // Using integer arithmetic. For better accuracy, could use float or weighted average.
            // uint8_t gray = (uint8_t)(((int)p->red + (int)p->green + (int)p->blue) / 3);

            // Using standard luminance calculation (often visually better)
            // Y = 0.299*R + 0.587*G + 0.114*B
            // Use integer approximation for speed, e.g., (2*R + 5*G + 1*B) / 8 or similar
            // Or floating point:
            float gray_f = 0.299f * p->red + 0.587f * p->green + 0.114f * p->blue;
            uint8_t gray = clamp_u8((int)(gray_f + 0.5f)); // Add 0.5 for rounding

            p->red = gray;
            p->green = gray;
            p->blue = gray;
        }
    }
     printf("Grayscale filter applied.\n");
}

/*
 * Adjust the brightness of the image.
 */
void bmp24_brightness(t_bmp24 *img, int value) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply brightness to NULL image or image with no data.\n");
        return;
    }
    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            t_pixel *p = &img->data[y][x];
            p->red   = clamp_u8((int)p->red + value);
            p->green = clamp_u8((int)p->green + value);
            p->blue  = clamp_u8((int)p->blue + value);
        }
    }
    printf("Brightness adjusted by %d.\n", value);
}

// --- Features: Convolution Filters (Part 2.6) ---

/*
 * Applies a convolution kernel to a specific pixel (x, y).
 * Reads neighbor pixel values from original_data (a copy of the image before filtering).
 * Returns the resulting pixel value.
 */
t_pixel bmp24_convolution(t_bmp24 *img, int x, int y, float **kernel, int kernelSize, t_pixel **original_data) {
    t_pixel result = {0, 0, 0}; // Initialize result pixel to black
    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    int kernelRadius = kernelSize / 2;

    // Ensure original_data is valid
    if (!original_data) {
        fprintf(stderr, "Error: original_data is NULL in bmp24_convolution.\n");
        return result; // Return black
    }

    // Apply the kernel to the neighborhood
    for (int ky = -kernelRadius; ky <= kernelRadius; ++ky) {
        int ny = y + ky; // Neighbor y coordinate
        for (int kx = -kernelRadius; kx <= kernelRadius; ++kx) {
            int nx = x + kx; // Neighbor x coordinate

            // Get the kernel value
            float k = kernel[ky + kernelRadius][kx + kernelRadius];

            // Get the neighbor pixel from the original data
            // Boundary handling: Use the pixel itself if out of bounds (or clamp/mirror)
            // Simple approach: Clamp coordinates to image boundaries
            int clamped_nx = nx;
            int clamped_ny = ny;
            if (clamped_nx < 0) clamped_nx = 0;
            if (clamped_nx >= img->width) clamped_nx = img->width - 1;
            if (clamped_ny < 0) clamped_ny = 0;
            if (clamped_ny >= img->height) clamped_ny = img->height - 1;

            t_pixel neighbor_pixel = original_data[clamped_ny][clamped_nx];

            // Accumulate weighted sum for each channel
            sum_r += (float)neighbor_pixel.red * k;
            sum_g += (float)neighbor_pixel.green * k;
            sum_b += (float)neighbor_pixel.blue * k;
        }
    }

    // Clamp the results and assign to the result pixel
    result.red   = clamp_u8((int)(sum_r + 0.5f));
    result.green = clamp_u8((int)(sum_g + 0.5f));
    result.blue  = clamp_u8((int)(sum_b + 0.5f));

    return result;
}

/*
 * Applies a given convolution filter (kernel) to the entire image.
 * Creates a copy of the original pixel data to avoid modifying pixels
 * that are needed for subsequent calculations.
 */
void bmp24_applyFilter(t_bmp24 *img, float **kernel, int kernelSize) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot apply filter to NULL image or image with no data.\n");
        return;
    }
    if (!kernel || kernelSize <= 0 || kernelSize % 2 == 0) {
        fprintf(stderr, "Error: Invalid kernel provided for filtering.\n");
        return;
    }

    int width = img->width;
    int height = img->height;

    // 1. Create a temporary copy of the original pixel data
    t_pixel **original_data = bmp24_allocateDataPixels(width, height);
    if (!original_data) {
        fprintf(stderr, "Error: Failed to allocate buffer for original pixel data in applyFilter.\n");
        return;
    }
    // Copy data row by row (or use memcpy on the contiguous block if allocated that way)
    for(int y = 0; y < height; ++y) {
         memcpy(original_data[y], img->data[y], width * sizeof(t_pixel));
    }

    // 2. Iterate through each pixel of the image
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 3. Calculate the new pixel value using convolution on the *original* data
            img->data[y][x] = bmp24_convolution(img, x, y, kernel, kernelSize, original_data);
        }
    }

    // 4. Free the temporary copy
    bmp24_freeDataPixels(original_data, height);

    printf("Convolution filter applied (kernel size %d).\n", kernelSize);
}

// --- Helper Functions for Kernel Management ---

// Allocates memory for a kernelSize x kernelSize float kernel
static float **allocateKernel(int kernelSize) {
    if (kernelSize <= 0 || kernelSize % 2 == 0) return NULL;
    float **kernel = (float **)malloc(kernelSize * sizeof(float *));
    if (!kernel) return NULL;
    kernel[0] = (float *)calloc(kernelSize * kernelSize, sizeof(float)); // Use calloc for zero initialization
    if (!kernel[0]) {
        free(kernel);
        return NULL;
    }
    for (int i = 1; i < kernelSize; ++i) {
        kernel[i] = kernel[i - 1] + kernelSize;
    }
    return kernel;
}

// Frees memory allocated for a kernel
static void freeKernel(float **kernel) {
    if (!kernel) return;
    if (kernel[0]) free(kernel[0]);
    free(kernel);
}

// --- Specific Filter Implementations (using bmp24_applyFilter) ---

void bmp24_boxBlur(t_bmp24 *img) {
    const int kernelSize = 3;
    float **kernel = allocateKernel(kernelSize);
    if (!kernel) {
        perror("Failed to allocate kernel for box blur");
        return;
    }

    float val = 1.0f / 9.0f;
    for (int i = 0; i < kernelSize; ++i) {
        for (int j = 0; j < kernelSize; ++j) {
            kernel[i][j] = val;
        }
    }

    bmp24_applyFilter(img, kernel, kernelSize);
    freeKernel(kernel);
}

void bmp24_gaussianBlur(t_bmp24 *img) {
    const int kernelSize = 3;
    float **kernel = allocateKernel(kernelSize);
     if (!kernel) {
        perror("Failed to allocate kernel for gaussian blur");
        return;
    }

    float factor = 1.0f / 16.0f;
    kernel[0][0] = 1.0f * factor; kernel[0][1] = 2.0f * factor; kernel[0][2] = 1.0f * factor;
    kernel[1][0] = 2.0f * factor; kernel[1][1] = 4.0f * factor; kernel[1][2] = 2.0f * factor;
    kernel[2][0] = 1.0f * factor; kernel[2][1] = 2.0f * factor; kernel[2][2] = 1.0f * factor;

    bmp24_applyFilter(img, kernel, kernelSize);
    freeKernel(kernel);
}

void bmp24_outline(t_bmp24 *img) {
    const int kernelSize = 3;
    float **kernel = allocateKernel(kernelSize);
    if (!kernel) {
        perror("Failed to allocate kernel for outline");
        return;
    }

    kernel[0][0] = -1.0f; kernel[0][1] = -1.0f; kernel[0][2] = -1.0f;
    kernel[1][0] = -1.0f; kernel[1][1] =  8.0f; kernel[1][2] = -1.0f;
    kernel[2][0] = -1.0f; kernel[2][1] = -1.0f; kernel[2][2] = -1.0f;

    bmp24_applyFilter(img, kernel, kernelSize);
    freeKernel(kernel);
}

void bmp24_emboss(t_bmp24 *img) {
     const int kernelSize = 3;
    float **kernel = allocateKernel(kernelSize);
    if (!kernel) {
        perror("Failed to allocate kernel for emboss");
        return;
    }

    kernel[0][0] = -2.0f; kernel[0][1] = -1.0f; kernel[0][2] = 0.0f;
    kernel[1][0] = -1.0f; kernel[1][1] =  1.0f; kernel[1][2] = 1.0f;
    kernel[2][0] =  0.0f; kernel[2][1] =  1.0f; kernel[2][2] = 2.0f;

    bmp24_applyFilter(img, kernel, kernelSize);
    freeKernel(kernel);
}

void bmp24_sharpen(t_bmp24 *img) {
    const int kernelSize = 3;
    float **kernel = allocateKernel(kernelSize);
    if (!kernel) {
        perror("Failed to allocate kernel for sharpen");
        return;
    }

    kernel[0][0] =  0.0f; kernel[0][1] = -1.0f; kernel[0][2] =  0.0f;
    kernel[1][0] = -1.0f; kernel[1][1] =  5.0f; kernel[1][2] = -1.0f;
    kernel[2][0] =  0.0f; kernel[2][1] = -1.0f; kernel[2][2] =  0.0f;

    bmp24_applyFilter(img, kernel, kernelSize);
    freeKernel(kernel);
}

// --- Placeholder Implementations for Remaining Functions ---
// void bmp24_boxBlur(t_bmp24 *img) { (void)img; /* TODO */ }
// void bmp24_gaussianBlur(t_bmp24 *img) { (void)img; /* TODO */ }
// void bmp24_outline(t_bmp24 *img) { (void)img; /* TODO */ }
// void bmp24_emboss(t_bmp24 *img) { (void)img; /* TODO */ }
// void bmp24_sharpen(t_bmp24 *img) { (void)img; /* TODO */ } 