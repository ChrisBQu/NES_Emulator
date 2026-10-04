#define _CRT_SECURE_NO_WARNINGS

// stb_image_write is a single-header library: exactly one .c file defines this before including it, to get the code
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../Utils/stb_image_write.h"

#include "Screenshot.h"
#include <direct.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Screenshots taken in the same second get a number on the end. This stops the search for a free name somewhere
#define MAX_SCREENSHOTS_PER_SECOND 100

// Helper function: whether a file already exists
static bool fileExists(const char* path) {
	FILE* f = fopen(path, "rb");
	if (f == NULL) { return false; }
	fclose(f);
	return true;
}

bool saveScreenshot(const uint8_t* rgb, int width, int height, char* path, size_t path_length) {
	if (rgb == NULL || path == NULL || path_length == 0) { return false; }

	// fopen can't create folders, so make it first. _mkdir just fails if it already exists, which is fine
	_mkdir(SCREENSHOT_FOLDER);

	// Name the file after the date and time, e.g. Screenshots/2026-10-04_21-15-03.png
	time_t now = time(NULL);
	struct tm local;
	localtime_s(&local, &now);
	char timestamp[32];
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &local);
	snprintf(path, path_length, "%s/%s.png", SCREENSHOT_FOLDER, timestamp);
	for (int n = 2; fileExists(path); n++) {
		if (n > MAX_SCREENSHOTS_PER_SECOND) {
			printf("Error: Too many screenshots this second.\n");
			return false;
		}
		snprintf(path, path_length, "%s/%s_%d.png", SCREENSHOT_FOLDER, timestamp, n);
	}

	if (!stbi_write_png(path, width, height, 3, rgb, width * 3)) {
		printf("Error: Could not write screenshot '%s'.\n", path);
		return false;
	}
	return true;
}