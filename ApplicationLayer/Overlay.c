#include "Overlay.h"
#include "OpenGL.h"
#include "../Screen.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A font that comes with Windows, so there's no font file to ship
#define OVERLAY_FONT_FILE "bahnschrift.ttf"
#define OVERLAY_FONT_PATH_LENGTH 512
#define OVERLAY_FONT_OPEN_SIZE 16

// Line height of overlay text, and the space between text and the edge of its background box, in NES pixels
#define OVERLAY_TEXT_HEIGHT 20
#define OVERLAY_TEXT_PADDING 2
#define OVERLAY_TEXT_BACKGROUND_ALPHA 160

// Distance between messages and the edge of the game picture, in NES pixels
#define OVERLAY_MESSAGE_MARGIN 6

// Elements fade out over this long before they are removed
#define OVERLAY_FADE_MS 300

// The overlay's shader: a texture multiplied by a color, which sets the fade and lets the white texture draw solid boxes
static const char* overlayVertexShader =
    "#version 330 core\n"
    "in vec2 a_position;\n"
    "in vec2 a_texCoord;\n"
    "out vec2 v_texCoord;\n"
    "void main() {\n"
    "    v_texCoord = a_texCoord;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";
static const char* overlayFragmentShader =
    "#version 330 core\n"
    "in vec2 v_texCoord;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    fragColor = texture(u_texture, v_texCoord) * u_color;\n"
    "}\n";

// Constructor
struct Overlay* createOverlay() {
    struct Overlay* overlay = (struct Overlay*)calloc(1, sizeof(struct Overlay));
    if (overlay == NULL) { return NULL; }
    overlay->nextId = OVERLAY_INVALID_ID + 1;
    overlay->messageId = OVERLAY_INVALID_ID;

    // Without a font the emulator still runs, text just isn't shown
    if (TTF_Init() != 0) {
        printf("Warning: SDL_ttf could not be initialized, overlay text won't be shown. SDL_ttf Error: %s\n", TTF_GetError());
        return overlay;
    }
    char path[OVERLAY_FONT_PATH_LENGTH];
    const char* windowsFolder = SDL_getenv("WINDIR");
    SDL_snprintf(path, sizeof(path), "%s\\Fonts\\%s", windowsFolder != NULL ? windowsFolder : "C:\\Windows", OVERLAY_FONT_FILE);
    overlay->font = TTF_OpenFont(path, OVERLAY_FONT_OPEN_SIZE);
    if (overlay->font == NULL) {
        printf("Warning: Could not open the overlay font, overlay text won't be shown. SDL_ttf Error: %s\n", TTF_GetError());
        return overlay;
    }
    // A font's line height is bigger than its point size. Measure it once, so text can be sized by line height
    overlay->fontSize = OVERLAY_FONT_OPEN_SIZE;
    overlay->fontPointsPerPixel = (float)OVERLAY_FONT_OPEN_SIZE / TTF_FontHeight(overlay->font);

    return overlay;
}

static void clearElement(struct OverlayElement* element) {
    if (element->texture != 0) { glDeleteTextures(1, &element->texture); }
    memset(element, 0, sizeof(struct OverlayElement));
}

void destroyOverlay(struct Overlay* overlay) {
    if (overlay == NULL) { return; }
    for (int i = 0; i < MAX_OVERLAY_ELEMENTS; i++) {
        clearElement(&overlay->elements[i]);
    }
    if (overlay->whiteTexture != 0) { glDeleteTextures(1, &overlay->whiteTexture); }
    if (overlay->shaderProgram != 0) { glDeleteProgram(overlay->shaderProgram); }
    if (overlay->font != NULL) { TTF_CloseFont(overlay->font); }
    TTF_Quit();
    free(overlay);
}

// Claim a free element slot. Returns NULL if they're all in use
static struct OverlayElement* addElement(struct Overlay* overlay, enum OverlayElementType type, float x, float y, Uint32 duration_ms) {
    for (int i = 0; i < MAX_OVERLAY_ELEMENTS; i++) {
        struct OverlayElement* element = &overlay->elements[i];
        if (element->active) { continue; }
        clearElement(element);
        element->active = true;
        element->id = overlay->nextId++;
        element->type = type;
        element->x = x;
        element->y = y;
        element->endTime = (duration_ms == OVERLAY_NO_EXPIRY) ? OVERLAY_NO_EXPIRY : SDL_GetTicks64() + duration_ms;
        return element;
    }
    printf("Warning: Overlay is full, an element was not shown.\n");
    return NULL;
}

int overlayAddText(struct Overlay* overlay, const char* text, float x, float y, Uint32 duration_ms) {
    if (overlay == NULL || text == NULL) { return OVERLAY_INVALID_ID; }
    struct OverlayElement* element = addElement(overlay, OVERLAY_ELEMENT_TEXT, x, y, duration_ms);
    if (element == NULL) { return OVERLAY_INVALID_ID; }
    SDL_strlcpy(element->text, text, MAX_OVERLAY_TEXT_LENGTH);
    return element->id;
}

void overlayRemove(struct Overlay* overlay, int id) {
    if (overlay == NULL || id == OVERLAY_INVALID_ID) { return; }
    for (int i = 0; i < MAX_OVERLAY_ELEMENTS; i++) {
        if (overlay->elements[i].active && overlay->elements[i].id == id) {
            clearElement(&overlay->elements[i]);
            return;
        }
    }
}

void overlayShowMessage(struct Overlay* overlay, const char* text) {
    if (overlay == NULL) { return; }
    overlayRemove(overlay, overlay->messageId);
    float x = OVERLAY_MESSAGE_MARGIN + OVERLAY_TEXT_PADDING;
    float y = DEFAULT_SCREEN_HEIGHT - OVERLAY_MESSAGE_MARGIN - OVERLAY_TEXT_PADDING - OVERLAY_TEXT_HEIGHT;
    overlay->messageId = overlayAddText(overlay, text, x, y, OVERLAY_MESSAGE_DURATION_MS);
}

// Render a text element's texture at the font size that matches the current window scale.
// Returns false if there's nothing to draw
static bool updateTextTexture(struct Overlay* overlay, struct OverlayElement* element, float scale) {
    if (overlay->font == NULL) { return false; }
    int fontSize = (int)(OVERLAY_TEXT_HEIGHT * scale * overlay->fontPointsPerPixel + 0.5f);
    if (fontSize < 1) { fontSize = 1; }
    if (element->texture != 0 && element->textureFontSize == fontSize) { return true; }

    if (element->texture != 0) {
        glDeleteTextures(1, &element->texture);
        element->texture = 0;
    }
    if (overlay->fontSize != fontSize) {
        if (TTF_SetFontSize(overlay->font, fontSize) != 0) { return false; }
        overlay->fontSize = fontSize;
    }

    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface* surface = TTF_RenderUTF8_Blended(overlay->font, element->text, white);
    if (surface == NULL) { return false; }
    // createOpenGLTexture takes ARGB8888, which is what SDL_ttf makes, but convert if it ever isn't
    if (surface->format->format != SDL_PIXELFORMAT_ARGB8888) {
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(surface);
        if (converted == NULL) { return false; }
        surface = converted;
    }
    element->texture = createOpenGLTexture(surface->w, surface->h, surface->pitch, surface->pixels, GL_LINEAR);
    // The texture is already at window resolution, so its size in NES pixels is its real size divided by the scale
    element->width = surface->w / scale;
    element->height = surface->h / scale;
    element->textureFontSize = fontSize;
    SDL_FreeSurface(surface);
    return element->texture != 0;
}

// Helper function: set up the overlay's shader and white texture. Returns false if the shader didn't build
static bool initOverlayOpenGL(struct Overlay* overlay) {
    overlay->shaderProgram = createOpenGLProgram(overlayVertexShader, overlayFragmentShader, "overlay");
    if (overlay->shaderProgram == 0) { return false; }
    overlay->uniformTexture = glGetUniformLocation(overlay->shaderProgram, "u_texture");
    overlay->uniformColor = glGetUniformLocation(overlay->shaderProgram, "u_color");
    uint32_t white = 0xFFFFFFFF;
    overlay->whiteTexture = createOpenGLTexture(1, 1, sizeof(white), &white, GL_NEAREST);
    return true;
}

// Helper function: draw a texture tinted by a color, over a rectangle given in window pixels from the top-left
static void drawRect(struct Overlay* overlay, struct Screen* screen, GLuint texture, float x, float y, float w, float h, float r, float g, float b, float a) {
    // Whole pixels keep text sharp. OpenGL counts the viewport from the bottom of the window
    int left = (int)lroundf(x);
    int top = (int)lroundf(y);
    int width = (int)lroundf(w);
    int height = (int)lroundf(h);
    glViewport(left, screen->outputHeight - top - height, width, height);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform4f(overlay->uniformColor, r, g, b, a);
    drawOpenGLQuad();
}

void drawOverlay(struct Overlay* overlay, struct Screen* screen) {
    if (overlay == NULL || screen == NULL) { return; }
    if (overlay->shaderProgram == 0 && !initOverlayOpenGL(overlay)) { return; }
    Uint64 now = SDL_GetTicks64();

    // drawFrame has worked out where the game picture sits in the window. Elements are placed relative to it,
    // but drawn in real window pixels instead of NES pixels, so text stays sharp
    float scale = (float)screen->pictureWidth / screen->width;
    float pictureX = (float)screen->pictureX;
    float pictureY = (float)screen->pictureY;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(overlay->shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(overlay->uniformTexture, 0);

    for (int i = 0; i < MAX_OVERLAY_ELEMENTS; i++) {
        struct OverlayElement* element = &overlay->elements[i];
        if (!element->active) { continue; }
        if (element->endTime != OVERLAY_NO_EXPIRY && now >= element->endTime) {
            clearElement(element);
            continue;
        }

        bool ready = false;
        switch (element->type) {
            case OVERLAY_ELEMENT_TEXT:
                ready = updateTextTexture(overlay, element, scale);
                break;
        }
        if (!ready) { continue; }

        // Fade out just before being removed
        float alpha = 1.0f;
        if (element->endTime != OVERLAY_NO_EXPIRY && element->endTime - now < OVERLAY_FADE_MS) {
            alpha = (float)(element->endTime - now) / OVERLAY_FADE_MS;
        }

        float x = pictureX + element->x * scale;
        float y = pictureY + element->y * scale;
        float w = element->width * scale;
        float h = element->height * scale;

        // Text gets a dark box behind it so it can be read over any background
        if (element->type == OVERLAY_ELEMENT_TEXT) {
            float padding = OVERLAY_TEXT_PADDING * scale;
            float boxAlpha = alpha * OVERLAY_TEXT_BACKGROUND_ALPHA / 255.0f;
            drawRect(overlay, screen, overlay->whiteTexture, x - padding, y - padding, w + padding * 2, h + padding * 2, 0.0f, 0.0f, 0.0f, boxAlpha);
        }

        drawRect(overlay, screen, element->texture, x, y, w, h, 1.0f, 1.0f, 1.0f, alpha);
    }
}