#include "Overlay.h"
#include "Screen.h"
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
    if (element->texture != NULL) { SDL_DestroyTexture(element->texture); }
    memset(element, 0, sizeof(struct OverlayElement));
}

void destroyOverlay(struct Overlay* overlay) {
    if (overlay == NULL) { return; }
    for (int i = 0; i < MAX_OVERLAY_ELEMENTS; i++) {
        clearElement(&overlay->elements[i]);
    }
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
static bool updateTextTexture(struct Overlay* overlay, struct OverlayElement* element, SDL_Renderer* renderer, float scale) {
    if (overlay->font == NULL) { return false; }
    int fontSize = (int)(OVERLAY_TEXT_HEIGHT * scale * overlay->fontPointsPerPixel + 0.5f);
    if (fontSize < 1) { fontSize = 1; }
    if (element->texture != NULL && element->textureFontSize == fontSize) { return true; }

    if (element->texture != NULL) {
        SDL_DestroyTexture(element->texture);
        element->texture = NULL;
    }
    if (overlay->fontSize != fontSize) {
        if (TTF_SetFontSize(overlay->font, fontSize) != 0) { return false; }
        overlay->fontSize = fontSize;
    }

    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface* surface = TTF_RenderUTF8_Blended(overlay->font, element->text, white);
    if (surface == NULL) { return false; }
    element->texture = SDL_CreateTextureFromSurface(renderer, surface);
    // The texture is already at window resolution, so its size in NES pixels is its real size divided by the scale
    element->width = surface->w / scale;
    element->height = surface->h / scale;
    element->textureFontSize = fontSize;
    SDL_FreeSurface(surface);
    return element->texture != NULL;
}

void drawOverlay(struct Overlay* overlay, struct Screen* screen) {
    if (overlay == NULL || screen == NULL) { return; }
    SDL_Renderer* renderer = screen->screenRenderer;
    Uint64 now = SDL_GetTicks64();

    // Work out where the game picture sits in the window, the same way SDL_RenderSetLogicalSize fits it
    int outputWidth, outputHeight;
    if (SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight) != 0) { return; }
    float scale = SDL_min((float)outputWidth / screen->width, (float)outputHeight / screen->height);
    float pictureX = (outputWidth - screen->width * scale) / 2;
    float pictureY = (outputHeight - screen->height * scale) / 2;

    // Draw in real window pixels instead of NES pixels, so text stays sharp
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

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
                ready = updateTextTexture(overlay, element, renderer, scale);
                break;
        }
        if (!ready) { continue; }

        // Fade out just before being removed
        Uint8 alpha = 255;
        if (element->endTime != OVERLAY_NO_EXPIRY && element->endTime - now < OVERLAY_FADE_MS) {
            alpha = (Uint8)(255 * (element->endTime - now) / OVERLAY_FADE_MS);
        }

        SDL_FRect dest = { pictureX + element->x * scale, pictureY + element->y * scale, element->width * scale, element->height * scale };

        // Text gets a dark box behind it so it can be read over any background
        if (element->type == OVERLAY_ELEMENT_TEXT) {
            float padding = OVERLAY_TEXT_PADDING * scale;
            SDL_FRect box = { dest.x - padding, dest.y - padding, dest.w + padding * 2, dest.h + padding * 2 };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(alpha * OVERLAY_TEXT_BACKGROUND_ALPHA / 255));
            SDL_RenderFillRectF(renderer, &box);
        }

        SDL_SetTextureAlphaMod(element->texture, alpha);
        SDL_RenderCopyF(renderer, element->texture, NULL, &dest);
    }

    // Back to NES pixels for the next frame
    SDL_RenderSetLogicalSize(renderer, screen->width, screen->height);
}