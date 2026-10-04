#ifndef OVERLAY_H
#define OVERLAY_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

// Declared here so the functions below can take a pointer to it without including its header
struct Screen;

#define MAX_OVERLAY_ELEMENTS 32
#define MAX_OVERLAY_TEXT_LENGTH 128

// Returned when an element couldn't be added. Removing it does nothing
#define OVERLAY_INVALID_ID 0

// Pass as the duration for elements that should stay until they are removed
#define OVERLAY_NO_EXPIRY 0

#define OVERLAY_MESSAGE_DURATION_MS 2000
#define OVERLAY_THUMBNAIL_DURATION_MS 3000

enum OverlayElementType {
    OVERLAY_ELEMENT_TEXT,
    OVERLAY_ELEMENT_IMAGE,
    OVERLAY_ELEMENT_BOX,    // A solid colored rectangle
};

// Elements are drawn a layer at a time, lowest first, so higher layers cover lower ones. New elements go on the middle layer
#define OVERLAY_LAYER_BACK 0
#define OVERLAY_LAYER_MIDDLE 1
#define OVERLAY_LAYER_FRONT 2
#define NUMBER_OF_OVERLAY_LAYERS 3

// Something drawn on top of the game: a texture, where to put it, and how long to keep it.
// The texture is built while drawing, since that's when the window size is known
struct OverlayElement {
    bool active;
    int id;
    enum OverlayElementType type;

    // Position of the top-left corner and size, in NES pixels relative to the game picture
    float x;
    float y;
    float width;
    float height;

    // SDL_GetTicks64() time when the element is removed, or OVERLAY_NO_EXPIRY to keep it
    Uint64 endTime;

    // OpenGL texture name (a GLuint), or 0 if it hasn't been built yet
    unsigned int texture;

    // Text elements are rendered at the window's real resolution so they stay sharp,
    // so the texture is rebuilt whenever the font size needed for the window changes
    char text[MAX_OVERLAY_TEXT_LENGTH];
    int textureFontSize;

    // Image elements keep their pixels (ARGB8888) until the texture is built, then let them go
    Uint32* pixels;
    int pixelWidth;
    int pixelHeight;

    // Width of a white border drawn around the element, in NES pixels. 0 for none
    float borderSize;

    // 1 is fully visible, 0 is invisible. Multiplied with the fade-out
    float opacity;

    // Box elements' color, each part from 0 to 1. The alpha is multiplied with the opacity
    float color[4];

    int layer;
};

struct Overlay {
    TTF_Font* font;
    // The size the font is currently set to, and how many points give one pixel of line height
    int fontSize;
    float fontPointsPerPixel;

    int nextId;
    // The elements showing the latest message and screenshot thumbnail, so new ones can replace them
    int messageId;
    int thumbnailId;
    struct OverlayElement elements[MAX_OVERLAY_ELEMENTS];

    // The overlay's own shader, separate from the screen shader so screen effects don't apply to it. It draws a texture
    // multiplied by a color. Solid boxes use a plain white texture. Created on the first draw, once OpenGL is running
    unsigned int shaderProgram;
    int uniformTexture;
    int uniformColor;
    unsigned int whiteTexture;
};

struct Overlay* createOverlay();
void destroyOverlay(struct Overlay* overlay);

// Add text with its top-left corner at (x, y) in NES pixels. Returns an id that can be passed to overlayRemove
int overlayAddText(struct Overlay* overlay, const char* text, float x, float y, Uint32 duration_ms);
void overlayRemove(struct Overlay* overlay, int id);

// Add an image from RGB pixels (3 bytes each, top row first), which are copied. It's drawn with its top-left corner at
// (x, y) and stretched to width x height, all in NES pixels. Returns an id that can be passed to overlayRemove
int overlayAddImage(struct Overlay* overlay, const uint8_t* rgb, int pixelWidth, int pixelHeight, float x, float y, float width, float height, Uint32 duration_ms);

// The same, from ARGB8888 pixels (0xAARRGGBB, like the screen's framebuffer)
int overlayAddImageARGB(struct Overlay* overlay, const Uint32* argb, int pixelWidth, int pixelHeight, float x, float y, float width, float height, Uint32 duration_ms);

// Change how an element looks after it's been added: a white border around it (size in NES pixels, 0 for none),
// and how visible it is (1 fully, 0 not at all)
void overlaySetBorder(struct Overlay* overlay, int id, float size);
void overlaySetOpacity(struct Overlay* overlay, int id, float opacity);

// Add a solid colored rectangle, with its top-left corner at (x, y) and size in NES pixels. Color parts go from 0 to 1
int overlayAddBox(struct Overlay* overlay, float x, float y, float width, float height, float r, float g, float b, float a, Uint32 duration_ms);

// Move an element so its top-left corner is at (x, y) in NES pixels
void overlaySetPosition(struct Overlay* overlay, int id, float x, float y);

// Choose which layer an element is drawn on (OVERLAY_LAYER_BACK, _MIDDLE or _FRONT)
void overlaySetLayer(struct Overlay* overlay, int id, int layer);

// Show a short message in the bottom-left corner, replacing the previous one
void overlayShowMessage(struct Overlay* overlay, const char* text);

// Show a small copy of a screenshot (RGB pixels, as for overlayAddImage) in the bottom-right corner, replacing the previous one
void overlayShowThumbnail(struct Overlay* overlay, const uint8_t* rgb, int pixelWidth, int pixelHeight);

// Draw the overlay over the game. Call after drawFrame and before presentFrame
void drawOverlay(struct Overlay* overlay, struct Screen* screen);

#endif