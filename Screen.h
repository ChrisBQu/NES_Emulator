#ifndef SCREEN_H
#define SCREEN_H

#include <SDL.h>

#define DEFAULT_SCREEN_WIDTH 256
#define DEFAULT_SCREEN_HEIGHT 240
#define DEFAULT_SCREEN_SCALE 2

struct Screen {
    int width;
    int height;

    SDL_Renderer* screenRenderer;

    // The emulator draws into this buffer one pixel at a time, and it is uploaded to screenTexture once per frame.
    // Pixels are ARGB8888 (0xAARRGGBB), matching the texture format
    uint32_t* framebuffer;
    SDL_Texture* screenTexture;
};

struct Screen* createScreen(int width, int height);

void clearScreen(struct Screen* screen);
void drawFrame(struct Screen* screen);
void presentFrame(struct Screen* screen);
void destroyScreen(struct Screen* screen);

#endif