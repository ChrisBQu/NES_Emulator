#ifndef NF_PALETTE_H
#define NF_PALETTE_H

#include <stdio.h>
#include <stdint.h>

// A structure to represent a pixel that will be passed through the console's output function to the SDL layer
struct NF_Pixel {
	int16_t x;
	int16_t y;
	int8_t r;
	int8_t g;
	int8_t b;
};

// Get one of the colors used by the NES, as an array in (R, G, B) format
const uint8_t* NF_getNESColor(uint8_t index);

#endif