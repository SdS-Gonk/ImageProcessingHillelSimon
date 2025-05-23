#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "bmp8.h"  // For t_bmp8 type
#include "bmp24.h" // For t_bmp24 type

// --- Part 3.3: 8-bit Grayscale Histogram Equalization ---

/**
 * @brief Computes the histogram of an 8-bit grayscale image.
 * @param img Pointer to the t_bmp8 image.
 * @return Pointer to an array of 256 unsigned integers representing the histogram.
 *         The caller is responsible for freeing this array.
 *         Returns NULL on error.
 */
unsigned int *bmp8_computeHistogram(t_bmp8 *img);

/**
 * @brief Computes the normalized cumulative distribution function (CDF) from a histogram.
 * @param hist Pointer to the histogram array (size 256).
 * @param numPixels The total number of pixels in the image (img->width * img->height).
 * @return Pointer to an array of 256 unsigned integers representing the lookup table
 *         for equalization (mapping original gray level to equalized gray level).
 *         The caller is responsible for freeing this array.
 *         Returns NULL on error or if hist is NULL.
 */
unsigned int *bmp8_computeEqualizedHistogram(unsigned int *hist, unsigned int numPixels);

/**
 * @brief Applies histogram equalization to an 8-bit grayscale image in place.
 * @param img Pointer to the t_bmp8 image to be equalized.
 */
void bmp8_equalize(t_bmp8 *img);

// --- Part 3.4: 24-bit Color Histogram Equalization ---

/**
 * @brief Applies histogram equalization to a 24-bit color image in place.
 *        Converts to YUV, equalizes the Y (luminance) channel, converts back to RGB.
 * @param img Pointer to the t_bmp24 image to be equalized.
 */
void bmp24_equalize(t_bmp24 *img);


#endif // HISTOGRAM_H 