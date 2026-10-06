#ifndef SCREEN_H
#define SCREEN_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define DEFAULT_SCREEN_WIDTH 256
#define DEFAULT_SCREEN_HEIGHT 240
#define DEFAULT_SCREEN_SCALE 2

// The screen shaders are loaded from these files in the Shaders folder next to the executable. They all share the
// vertex shader. If a shader's files are missing or don't compile, a built-in shader that shows the picture unchanged is used instead
#define SCREEN_VERTEX_SHADER_FILE "screen.vert"
#define SCREEN_FRAGMENT_SHADER_FILE "screen.frag"
#define CRT_FRAGMENT_SHADER_FILE "crt.frag"

enum ScreenShader {
    SCREEN_SHADER_NORMAL,   // screen.frag
    SCREEN_SHADER_CRT,      // crt.frag
    NUMBER_OF_SCREEN_SHADERS
};

// The CRT shader's settings. See crt.frag for what each one does
struct CrtSettings {
    float hardScan;     // Hardness of the scanlines. More negative is harder
    float hardPix;      // Hardness of the pixels in a scanline. More negative is harder
    float warpH;        // Horizontal warp. 0 is none
    float warpV;        // Vertical warp. 0 is none
    float maskDark;     // Brightness of the shadow mask's dark stripes
    float maskLight;    // Brightness of the shadow mask's light stripes
};

// A built screen shader, and where its uniforms are. OpenGL names (GLuint and GLint) are kept as plain ints so this
// header doesn't need the OpenGL headers
struct ScreenShaderProgram {
    unsigned int program;
    int uniformTexture;
    int uniformSourceSize;
    int uniformOutputSize;
    int uniformTime;
    int uniformHardScan;
    int uniformHardPix;
    int uniformWarp;
    int uniformMaskDark;
    int uniformMaskLight;
};

struct Screen {
    int width;
    int height;

    SDL_GLContext glContext;

    // The emulator draws into this buffer one pixel at a time, and it is uploaded to screenTexture once per frame.
    // Pixels are ARGB8888 (0xAARRGGBB)
    uint32_t* framebuffer;

    // OpenGL texture name (a GLuint)
    unsigned int screenTexture;

    // Every screen shader is built at startup, so switching between them is instant
    struct ScreenShaderProgram shaders[NUMBER_OF_SCREEN_SHADERS];
    enum ScreenShader currentShader;
    struct CrtSettings crtSettings;

    // Updated by drawFrame: the size of the window's drawing area, and where the game picture sits in it,
    // in window pixels from the top-left. The picture keeps its shape, with black bars filling the leftover space
    int outputWidth;
    int outputHeight;
    int pictureX;
    int pictureY;
    int pictureWidth;
    int pictureHeight;
};

struct Screen* createScreen(int width, int height);

void clearScreen(struct Screen* screen);
void drawFrame(struct Screen* screen);
void presentFrame(struct Screen* screen);

// Choose which shader the game picture is drawn through, from the next frame on
void setScreenShader(struct Screen* screen, enum ScreenShader shader);

// Set the CRT shader's settings, from the next frame on
void setCrtSettings(struct Screen* screen, const struct CrtSettings* settings);

// Get the last frame the NES drew as RGB bytes, top row first, without black bars or the overlay. With applyShader, it's
// drawn through the current shader at the size it's shown on screen. Without, it's the NES's own 256x240 picture.
// The size is written into width and height. Returns NULL if it failed. Free the result with free()
uint8_t* readScreenPixels(struct Screen* screen, bool applyShader, int* width, int* height);
void destroyScreen(struct Screen* screen);

#endif