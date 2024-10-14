#ifndef NF_PALETTE_H
#define NF_PALETTE_H

#include <stdio.h>
#include <stdint.h>

// A structure to represent a pixel that will be passed through the console's output function to the SDL layer
struct NF_Pixel {
    int16_t x;   // X coordinate of the pixel
    int16_t y;   // Y coordinate of the pixel
    uint8_t r;   // Red component of the pixel (0-255)
    uint8_t g;   // Green component of the pixel (0-255)
    uint8_t b;   // Blue component of the pixel (0-255)
};


// Get one of the colors used by the NES, as an array in (R, G, B) format
const uint8_t* NF_getNESColor(uint8_t index);

#endif