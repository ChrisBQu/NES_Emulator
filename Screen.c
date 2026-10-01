#include "Screen.h"
#include "ApplicationLayer/Window.h"

// Constructor
struct Screen* createScreen(int width, int height) {
    struct Screen* screen = (struct Screen*)malloc(sizeof(struct Screen));
    if (screen == NULL) { return NULL; }
    screen->width = width;
    screen->height = height;
    screen->framebuffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    if (screen->framebuffer == NULL) { 
        printf("Error: Could not allocate frame buffer. Out off memory?\n");
        free(screen);
        return NULL; 
    }

    // Create the renderer
    int v = SDL_Init(SDL_INIT_VIDEO);
    screen->screenRenderer = SDL_CreateRenderer(getWindow(), -1, SDL_RENDERER_ACCELERATED);
    if (screen->screenRenderer == NULL) {
        printf("Error: SDL Renderer could not be created. SDL Error: %s\n", SDL_GetError());
        return NULL;
    }

    // Scale the picture to fit the window without changing its shape, centered with black bars filling the leftover space
    SDL_RenderSetLogicalSize(screen->screenRenderer, width, height);

    // A streaming texture is updated from the CPU every frame
    screen->screenTexture = SDL_CreateTexture(screen->screenRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, screen->width, screen->height);
    if (screen->screenTexture == NULL) {
        printf("Error: Screen texture could not be created. SDL Error: %s\n", SDL_GetError());
        return NULL;
    }

    return screen;
}

void clearScreen(struct Screen* screen) {
    if (screen == NULL) { return; }
    SDL_SetRenderDrawColor(screen->screenRenderer, 0, 0, 0, 255);
    SDL_RenderClear(screen->screenRenderer);
}

void presentFrame(struct Screen* screen) {
    if (screen == NULL) { return; }
    SDL_UpdateTexture(screen->screenTexture, NULL, screen->framebuffer, screen->width * sizeof(uint32_t));
    clearScreen(screen);
    SDL_RenderCopy(screen->screenRenderer, screen->screenTexture, NULL, NULL);
    SDL_RenderPresent(screen->screenRenderer);
}

void destroyScreen(struct Screen* screen) {
    if (screen == NULL) { return; }
    if (screen->framebuffer != NULL) { free(screen->framebuffer); }
    if (screen->screenTexture != NULL) { SDL_DestroyTexture(screen->screenTexture); }
    if (screen->screenRenderer != NULL) { SDL_DestroyRenderer(screen->screenRenderer); }
    free(screen);
}