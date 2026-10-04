#ifndef OVERLAY_H
#define OVERLAY_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

// Declared here so the functions below can take a pointer to it without including its header
struct Screen;

#define MAX_OVERLAY_ELEMENTS 16
#define MAX_OVERLAY_TEXT_LENGTH 128

// Returned when an element couldn't be added. Removing it does nothing
#define OVERLAY_INVALID_ID 0

// Pass as the duration for elements that should stay until they are removed
#define OVERLAY_NO_EXPIRY 0

#define OVERLAY_MESSAGE_DURATION_MS 2000

enum OverlayElementType {
    OVERLAY_ELEMENT_TEXT,
};

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
};

struct Overlay {
    TTF_Font* font;
    // The size the font is currently set to, and how many points give one pixel of line height
    int fontSize;
    float fontPointsPerPixel;

    int nextId;
    // The element showing the latest message, so a new message can replace it
    int messageId;
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

// Show a short message in the bottom-left corner, replacing the previous one
void overlayShowMessage(struct Overlay* overlay, const char* text);

// Draw the overlay over the game. Call after drawFrame and before presentFrame
void drawOverlay(struct Overlay* overlay, struct Screen* screen);

#endif