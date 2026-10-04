#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCREENSHOT_FOLDER "Screenshots"
#define MAX_SCREENSHOT_PATH_LENGTH 128

// Save RGB pixels (3 bytes each, top row first) as a PNG in the Screenshots folder, named after the date and time.
// Get the pixels with readScreenPixels, which decides whether the filter is applied. The file's path is written into
// path. Returns true if it was saved
bool saveScreenshot(const uint8_t* rgb, int width, int height, char* path, size_t path_length);

#endif