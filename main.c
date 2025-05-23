#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For tolower

#include "bmp8.h"
#include "bmp24.h"
#include "histogram.h"

// --- Global State ---
t_bmp8* current_img8 = NULL;
t_bmp24* current_img24 = NULL;
char current_filename[256] = "";
int image_loaded = 0; // 0: none, 8: 8-bit loaded, 24: 24-bit loaded

// --- Helper Functions ---

// Clear input buffer
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Get integer input from user
int get_int_input(const char* prompt) {
    int value;
    int result;
    do {
        printf("%s", prompt);
        result = scanf("%d", &value);
        clear_input_buffer();
        if (result != 1) {
            printf("Invalid input. Please enter an integer.\n");
        }
    } while (result != 1);
    return value;
}

// Get string input from user
void get_string_input(const char* prompt, char* buffer, size_t buffer_size) {
    printf("%s", prompt);
    if (fgets(buffer, buffer_size, stdin)) {
        // Remove trailing newline character if present
        buffer[strcspn(buffer, "\n")] = 0;
    } else {
        // Handle potential fgets error
        buffer[0] = '\0';
        clear_input_buffer(); // Clear buffer in case of error
    }
}

// Free currently loaded image
void free_current_image() {
    if (image_loaded == 8 && current_img8) {
        bmp8_free(current_img8);
        current_img8 = NULL;
    } else if (image_loaded == 24 && current_img24) {
        bmp24_free(current_img24);
        current_img24 = NULL;
    }
    image_loaded = 0;
    current_filename[0] = '\0';
}

// --- Menu Functions ---

void menu_open_image() {
    char filename[256];
    get_string_input("Enter image file path (BMP): ", filename, sizeof(filename));

    // Free previous image if any
    free_current_image();

    // Try loading as 24-bit first (more common for color)
    current_img24 = bmp24_loadImage(filename);
    if (current_img24) {
        image_loaded = 24;
        strncpy(current_filename, filename, sizeof(current_filename) - 1);
        current_filename[sizeof(current_filename) - 1] = '\0';
        printf("24-bit image loaded successfully.\n");
        return;
    }

    // If 24-bit fails, try loading as 8-bit
    // Reset error messages or indicate trying 8-bit
    // printf("Failed to load as 24-bit, trying 8-bit...\n");
    current_img8 = bmp8_loadImage(filename);
    if (current_img8) {
        image_loaded = 8;
        strncpy(current_filename, filename, sizeof(current_filename) - 1);
        current_filename[sizeof(current_filename) - 1] = '\0';
        printf("8-bit image loaded successfully.\n");
    } else {
        // Both failed
        fprintf(stderr, "Failed to load image '%s' as either 8-bit or 24-bit BMP.\n", filename);
        image_loaded = 0;
    }
}

void menu_save_image() {
    if (!image_loaded) {
        printf("No image loaded to save.\n");
        return;
    }

    char filename[256];
    // Suggest a default filename based on the original
    char default_filename[300];
    char *dot = strrchr(current_filename, '.');
    if (dot) {
        snprintf(default_filename, sizeof(default_filename), "%.*s_modified%s", (int)(dot - current_filename), current_filename, dot);
    } else {
        snprintf(default_filename, sizeof(default_filename), "%s_modified.bmp", current_filename);
    }
    printf("Enter save file path (default: %s): ", default_filename);

    // Read input, allowing empty input to use default
    if (fgets(filename, sizeof(filename), stdin)) {
        filename[strcspn(filename, "\n")] = 0; // Remove newline
        if (strlen(filename) == 0) { // User pressed Enter, use default
            strncpy(filename, default_filename, sizeof(filename) - 1);
            filename[sizeof(filename)-1] = '\0';
        }
    } else {
        // Handle fgets error, maybe use default
        strncpy(filename, default_filename, sizeof(filename) - 1);
        filename[sizeof(filename)-1] = '\0';
        clear_input_buffer();
    }


    if (image_loaded == 8) {
        bmp8_saveImage(filename, current_img8);
    } else if (image_loaded == 24) {
        bmp24_saveImage(filename, current_img24);
    }
}

void menu_display_info() {
    if (!image_loaded) {
        printf("No image loaded.\n");
        return;
    }

    if (image_loaded == 8) {
        bmp8_printInfo(current_img8);
    } else if (image_loaded == 24) {
        // Need a bmp24_printInfo function - let's add it quickly
        printf("Image Info (24-bit):\n");
        printf("  Width: %d\n", current_img24->width);
        printf("  Height: %d\n", current_img24->height);
        printf("  Color Depth: %d\n", current_img24->colorDepth);
        // Add more info from headers if desired
        printf("  File Size (header): %u bytes\n", current_img24->header.size);
        printf("  Data Offset (header): %u\n", current_img24->header.offset);
        printf("  Pixel Data Size (header): %u bytes\n", current_img24->header_info.imagesize);
    }
}

// Forward declaration for filter menu
void menu_apply_filter();

// --- Main Menu --- 

void display_main_menu() {
    printf("\n--- Image Processing Menu ---\n");
    printf("1. Open Image\n");
    printf("2. Save Image\n");
    printf("3. Apply Filter/Operation\n");
    printf("4. Display Image Info\n");
    printf("5. Quit\n");
    printf("-----------------------------\n");
    if (image_loaded) {
        printf("Current Image: %s (%d-bit)\n", current_filename, image_loaded);
    } else {
        printf("Current Image: None\n");
    }
}

int main() {
    int choice;
    while (1) {
        display_main_menu();
        choice = get_int_input(">>> Your choice: ");

        switch (choice) {
            case 1:
                menu_open_image();
                break;
            case 2:
                menu_save_image();
                break;
            case 3:
                if (!image_loaded) {
                    printf("Please open an image first.\n");
                } else {
                    menu_apply_filter();
                }
                break;
            case 4:
                menu_display_info();
                break;
            case 5:
                printf("Exiting program.\n");
                free_current_image();
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }
    return 0; // Should not reach here
}

// --- Filter Sub-Menu ---

// Kernel allocation/freeing helpers (copied/adapted from bmp24.c)
static float **allocateKernel(int kernelSize) {
    if (kernelSize <= 0 || kernelSize % 2 == 0) return NULL;
    float **kernel = (float **)malloc(kernelSize * sizeof(float *));
    if (!kernel) return NULL;
    kernel[0] = (float *)calloc(kernelSize * kernelSize, sizeof(float));
    if (!kernel[0]) { free(kernel); return NULL; }
    for (int i = 1; i < kernelSize; ++i) { kernel[i] = kernel[i - 1] + kernelSize; }
    return kernel;
}
static void freeKernel(float **kernel) {
    if (!kernel) return;
    if (kernel[0]) free(kernel[0]);
    free(kernel);
}

void menu_apply_filter() {
    printf("\n--- Apply Filter/Operation ---\n");
    // Common operations
    printf("1. Negative\n");
    printf("2. Brightness\n");
    // Grayscale specific
    if (image_loaded == 8) {
        printf("3. Threshold (Black & White)\n");
    }
    // Color specific
    if (image_loaded == 24) {
        printf("3. Grayscale Conversion\n");
    }
    printf("4. Box Blur (3x3)\n");
    printf("5. Gaussian Blur (3x3)\n");
    printf("6. Sharpen (3x3)\n");
    printf("7. Outline (Edge Detection)\n");
    printf("8. Emboss (3x3)\n");
    printf("9. Histogram Equalization\n");
    printf("10. Return to Main Menu\n");
    printf("-----------------------------\n");

    int choice = get_int_input(">>> Filter choice: ");

    // Kernels (only allocate if needed)
    float **kernel = NULL;
    const int kernelSize = 3;

    switch (choice) {
        case 1: // Negative
            if (image_loaded == 8) bmp8_negative(current_img8);
            else bmp24_negative(current_img24);
            break;
        case 2: // Brightness
            {
                int value = get_int_input("Enter brightness adjustment value (-255 to 255): ");
                if (image_loaded == 8) bmp8_brightness(current_img8, value);
                else bmp24_brightness(current_img24, value);
            }
            break;
        case 3: // Threshold (8-bit) or Grayscale (24-bit)
            if (image_loaded == 8) {
                int threshold = get_int_input("Enter threshold value (0 to 255): ");
                bmp8_threshold(current_img8, threshold);
            } else {
                bmp24_grayscale(current_img24);
            }
            break;
        case 4: // Box Blur
            if (image_loaded == 8) {
                kernel = allocateKernel(kernelSize);
                if(kernel) {
                    float val = 1.0f / 9.0f;
                    for (int i=0; i<kernelSize; ++i) for(int j=0; j<kernelSize; ++j) kernel[i][j] = val;
                    bmp8_applyFilter(current_img8, kernel, kernelSize);
                    freeKernel(kernel);
                }
            } else {
                 bmp24_boxBlur(current_img24); // Uses its own kernel
            }
            break;
        case 5: // Gaussian Blur
             if (image_loaded == 8) {
                kernel = allocateKernel(kernelSize);
                 if(kernel) {
                    float factor = 1.0f / 16.0f;
                    kernel[0][0] = 1.0f*factor; kernel[0][1] = 2.0f*factor; kernel[0][2] = 1.0f*factor;
                    kernel[1][0] = 2.0f*factor; kernel[1][1] = 4.0f*factor; kernel[1][2] = 2.0f*factor;
                    kernel[2][0] = 1.0f*factor; kernel[2][1] = 2.0f*factor; kernel[2][2] = 1.0f*factor;
                    bmp8_applyFilter(current_img8, kernel, kernelSize);
                    freeKernel(kernel);
                 }
            } else {
                bmp24_gaussianBlur(current_img24);
            }
            break;
        case 6: // Sharpen
             if (image_loaded == 8) {
                kernel = allocateKernel(kernelSize);
                 if(kernel) {
                    kernel[0][0] =  0.0f; kernel[0][1] = -1.0f; kernel[0][2] =  0.0f;
                    kernel[1][0] = -1.0f; kernel[1][1] =  5.0f; kernel[1][2] = -1.0f;
                    kernel[2][0] =  0.0f; kernel[2][1] = -1.0f; kernel[2][2] =  0.0f;
                    bmp8_applyFilter(current_img8, kernel, kernelSize);
                    freeKernel(kernel);
                 }
            } else {
                bmp24_sharpen(current_img24);
            }
            break;
        case 7: // Outline
             if (image_loaded == 8) {
                kernel = allocateKernel(kernelSize);
                 if(kernel) {
                    kernel[0][0] = -1.0f; kernel[0][1] = -1.0f; kernel[0][2] = -1.0f;
                    kernel[1][0] = -1.0f; kernel[1][1] =  8.0f; kernel[1][2] = -1.0f;
                    kernel[2][0] = -1.0f; kernel[2][1] = -1.0f; kernel[2][2] = -1.0f;
                    bmp8_applyFilter(current_img8, kernel, kernelSize);
                    freeKernel(kernel);
                 }
            } else {
                bmp24_outline(current_img24);
            }
            break;
        case 8: // Emboss
            if (image_loaded == 8) {
                kernel = allocateKernel(kernelSize);
                 if(kernel) {
                    kernel[0][0] = -2.0f; kernel[0][1] = -1.0f; kernel[0][2] = 0.0f;
                    kernel[1][0] = -1.0f; kernel[1][1] =  1.0f; kernel[1][2] = 1.0f;
                    kernel[2][0] =  0.0f; kernel[2][1] =  1.0f; kernel[2][2] = 2.0f;
                    bmp8_applyFilter(current_img8, kernel, kernelSize);
                    freeKernel(kernel);
                 }
            } else {
                bmp24_emboss(current_img24);
            }
            break;
        case 9: // Histogram Equalization
             if (image_loaded == 8) {
                 bmp8_equalize(current_img8);
             } else {
                 bmp24_equalize(current_img24);
             }
             break;
        case 10: // Return
            printf("Returning to main menu.\n");
            break;
        default:
            printf("Invalid filter choice.\n");
            break;
    }
    // Free kernel if allocated by a case that failed (shouldn't happen with current structure)
    // if (kernel) freeKernel(kernel);
} 