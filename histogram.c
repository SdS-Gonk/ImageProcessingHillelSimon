#include "histogram.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // For memset
#include <math.h>   // For round, roundf

// --- Part 3.3: 8-bit Grayscale Histogram Equalization ---

/*
 * Computes the histogram of an 8-bit grayscale image.
 */
unsigned int *bmp8_computeHistogram(t_bmp8 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot compute histogram for NULL image or image with no data.\n");
        return NULL;
    }

    // Allocate histogram array (256 bins for 0-255 gray levels)
    unsigned int *hist = (unsigned int *)calloc(256, sizeof(unsigned int));
    if (!hist) {
        perror("Error allocating memory for histogram");
        return NULL;
    }

    // Iterate through pixel data and increment corresponding histogram bin
    for (unsigned int i = 0; i < img->dataSize; ++i) {
        hist[img->data[i]]++;
    }

    return hist;
}

/*
 * Computes the normalized cumulative distribution function (CDF) from a histogram,
 * resulting in the lookup table for equalization.
 * Renamed from bmp8_computeCDF to match header description and combine steps.
 */
unsigned int *bmp8_computeEqualizedHistogram(unsigned int *hist, unsigned int numPixels) {
    if (!hist || numPixels == 0) {
        fprintf(stderr, "Error: Invalid arguments for computeEqualizedHistogram.\n");
        return NULL;
    }

    // Allocate array for the equalized histogram / lookup table (LUT)
    unsigned int *hist_eq = (unsigned int *)malloc(256 * sizeof(unsigned int));
    if (!hist_eq) {
        perror("Error allocating memory for equalized histogram");
        return NULL;
    }

    // 1. Calculate the cumulative histogram (CDF)
    unsigned long long *cdf = (unsigned long long *)calloc(256, sizeof(unsigned long long));
    if (!cdf) {
         perror("Error allocating memory for CDF");
         free(hist_eq);
         return NULL;
    }

    cdf[0] = hist[0];
    for (int i = 1; i < 256; ++i) {
        cdf[i] = cdf[i - 1] + hist[i];
    }

    // Find the minimum non-zero CDF value (cdfmin)
    unsigned long long cdfmin = 0;
    for (int i = 0; i < 256; ++i) {
        if (cdf[i] > 0) {
            cdfmin = cdf[i];
            break;
        }
    }

    // Avoid division by zero if all pixels are the same value (or image is empty)
    // although numPixels==0 check above should catch empty image.
    // If numPixels == cdfmin, the denominator is zero.
    double denominator = (double)numPixels - cdfmin;
    if (denominator <= 0) {
        // Handle this case: maybe map all to a single value or return identity LUT
        // For simplicity, create an identity mapping
        fprintf(stderr, "Warning: Image has uniform color or issue with CDF calculation. Applying identity transform.\n");
        for(int i = 0; i < 256; ++i) {
             hist_eq[i] = i;
        }
    } else {
        // 2. Normalize the CDF using the formula to create the LUT
        double scale_factor = 255.0 / denominator;
        for (int i = 0; i < 256; ++i) {
            // Only apply formula if cdf[i] is non-zero (otherwise hist_eq[i] should map to 0)
            if (cdf[i] == 0) {
                hist_eq[i] = 0; // Or should map to the value for cdfmin?
                                // Let's map based on formula, which will be 0 if cdf[i] < cdfmin
            }
            // Ensure we don't subtract cdfmin if cdf[i] is less (though shouldn't happen with correct cdfmin)
            unsigned long long numerator = (cdf[i] >= cdfmin) ? (cdf[i] - cdfmin) : 0;
            hist_eq[i] = (unsigned int)round(numerator * scale_factor);
            // Clamp just in case of floating point inaccuracies
            if (hist_eq[i] > 255) hist_eq[i] = 255;
        }
    }

    free(cdf);
    return hist_eq;
}

/*
 * Applies histogram equalization to an 8-bit grayscale image in place.
 */
void bmp8_equalize(t_bmp8 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot equalize NULL image or image with no data.\n");
        return;
    }

    // 1. Compute histogram
    unsigned int *hist = bmp8_computeHistogram(img);
    if (!hist) return; // Error message already printed

    // 2. Compute equalized histogram (LUT)
    unsigned int numPixels = img->width * img->height;
    unsigned int *hist_eq = bmp8_computeEqualizedHistogram(hist, numPixels);
    if (!hist_eq) {
        free(hist);
        return; // Error message already printed
    }

    // 3. Apply the equalization LUT to the image data
    for (unsigned int i = 0; i < img->dataSize; ++i) {
        img->data[i] = (unsigned char)hist_eq[img->data[i]];
    }

    // 4. Free allocated memory
    free(hist);
    free(hist_eq);

    printf("Grayscale histogram equalization applied.\n");
}

// Helper function to clamp value between 0 and 255
static uint8_t clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

// Overload for float input (useful for YUV->RGB conversion)
static uint8_t clamp_u8_f(float value) {
    if (value < 0.0f) return 0;
    if (value > 255.0f) return 255;
    return (uint8_t)roundf(value); // Use roundf from math.h
}

// Structure to hold YUV components (using floats for precision during conversion)
typedef struct {
    float y;
    float u;
    float v;
} t_yuv;

// --- Part 3.4: 24-bit Color Histogram Equalization ---

/*
 * Applies histogram equalization to a 24-bit color image in place.
 */
void bmp24_equalize(t_bmp24 *img) {
     if (!img || !img->data) {
        fprintf(stderr, "Error: Cannot equalize NULL color image or image with no data.\n");
        return;
    }

    int width = img->width;
    int height = img->height;
    unsigned int numPixels = width * height;

    // 1. Allocate temporary storage for YUV components
    t_yuv *yuv_data = (t_yuv *)malloc(numPixels * sizeof(t_yuv));
    // Also allocate storage for the integer Y component [0-255] for histogramming
    uint8_t *y_int_data = (uint8_t *)malloc(numPixels * sizeof(uint8_t));
    if (!yuv_data || !y_int_data) {
        perror("Error allocating memory for YUV/Y data");
        free(yuv_data); // free() handles NULL
        free(y_int_data);
        return;
    }

    // 2. Convert RGB to YUV and store Y (int) and YUV (float)
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            t_pixel p = img->data[r][c];
            float R = (float)p.red;
            float G = (float)p.green;
            float B = (float)p.blue;

            // RGB to YUV conversion formulas (Eq. 3.3)
            float Y = 0.299f * R + 0.587f * G + 0.114f * B;
            float U = -0.14713f * R - 0.28886f * G + 0.436f * B;
            float V = 0.615f * R - 0.51499f * G - 0.10001f * B;

            int index = r * width + c;
            yuv_data[index].y = Y;
            yuv_data[index].u = U;
            yuv_data[index].v = V;

            // Store clamped integer Y [0-255] for histogram
            y_int_data[index] = clamp_u8_f(Y);
        }
    }

    // 3. Compute histogram of the integer Y component
    unsigned int *hist_y = (unsigned int *)calloc(256, sizeof(unsigned int));
    if (!hist_y) {
        perror("Error allocating memory for Y histogram");
        free(yuv_data);
        free(y_int_data);
        return;
    }
    for (unsigned int i = 0; i < numPixels; ++i) {
        hist_y[y_int_data[i]]++;
    }

    // 4. Compute equalized Y histogram (LUT)
    // Use the same function as for grayscale, passing the Y histogram
    unsigned int *hist_y_eq = bmp8_computeEqualizedHistogram(hist_y, numPixels);
    if (!hist_y_eq) {
        fprintf(stderr, "Error computing equalized Y histogram.\n");
        free(yuv_data);
        free(y_int_data);
        free(hist_y);
        return;
    }

    // 5. Apply Y equalization LUT to the *integer* Y data
    // (We'll use this look-up when converting back)
    // We don't need to modify yuv_data[i].y directly yet.

    // 6. Convert YUV back to RGB, using the *equalized* Y value
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            int index = r * width + c;
            // Get the original U and V components
            float U = yuv_data[index].u;
            float V = yuv_data[index].v;
            // Get the *original* integer Y value to look up its equalized value
            uint8_t original_y_int = y_int_data[index];
            // Get the *new* equalized Y value from the LUT
            float Y_eq = (float)hist_y_eq[original_y_int];

            // YUV to RGB conversion formulas (Eq. 3.4), using Y_eq
            float R_f = Y_eq + 1.13983f * V;
            float G_f = Y_eq - 0.39465f * U - 0.58060f * V;
            float B_f = Y_eq + 2.03211f * U;

            // Clamp and update the image pixel data
            img->data[r][c].red = clamp_u8_f(R_f);
            img->data[r][c].green = clamp_u8_f(G_f);
            img->data[r][c].blue = clamp_u8_f(B_f);
        }
    }

    // 7. Free allocated memory
    free(yuv_data);
    free(y_int_data);
    free(hist_y);
    free(hist_y_eq);

    printf("Color histogram equalization applied.\n");
} 