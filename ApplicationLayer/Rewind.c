#include "Rewind.h"
#include "Overlay.h"
#include "../Screen.h"
#include "../EmulationLayer/Bus.h"
#include "../EmulationLayer/PPU.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Layout of the thumbnail strip along the bottom of the picture, in NES pixels. Thumbnails shrink to fit them all,
// but never grow past the largest width. Their height follows the picture's shape
#define REWIND_MAX_SLOT_WIDTH 44.0f
#define REWIND_SLOT_GAP 1.0f
#define REWIND_STRIP_MARGIN 6.0f
#define REWIND_PANEL_PADDING 3.0f

// The panel behind the strip: light and mostly opaque, so the thumbnails stand out from the game
#define REWIND_PANEL_COLOR 0.85f, 0.85f, 0.85f, 0.9f

// The cursor is a white line with a dark outline, so it shows up on the light panel and on the thumbnails
#define REWIND_CURSOR_WIDTH 1.0f
#define REWIND_CURSOR_OUTLINE 0.5f

// The cursor moves REWIND_START_SPEED frames per update when left or right is first pressed, so a tap moves one frame.
// Once it's been held for REWIND_FAST_AFTER_UPDATES updates (about half a second), it moves REWIND_FAST_SPEED frames per update
#define REWIND_START_SPEED 1
#define REWIND_FAST_AFTER_UPDATES 30
#define REWIND_FAST_SPEED 8

#define REWIND_FRAMES_PER_SECOND 60.0988

// Constructor
struct Rewind* createRewind(int width, int height) {
    struct Rewind* rewind = (struct Rewind*)calloc(1, sizeof(struct Rewind));
    if (rewind == NULL) { return NULL; }
    rewind->width = width;
    rewind->height = height;
    rewind->nowPixels = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    if (rewind->nowPixels == NULL) {
        free(rewind);
        return NULL;
    }
    rewind->panelId = OVERLAY_INVALID_ID;
    for (int i = 0; i < REWIND_MAX_THUMBNAILS; i++) { rewind->thumbnailIds[i] = OVERLAY_INVALID_ID; }
    rewind->cursorOutlineId = OVERLAY_INVALID_ID;
    rewind->cursorId = OVERLAY_INVALID_ID;
    rewind->labelId = OVERLAY_INVALID_ID;
    return rewind;
}

void destroyRewind(struct Rewind* rewind) {
    if (rewind == NULL) { return; }
    for (int i = 0; i < REWIND_MAX_THUMBNAILS; i++) { free(rewind->thumbnails[i].pixels); }
    free(rewind->nowPixels);
    free(rewind);
}

void rewindTickFrame(struct Rewind* rewind, struct Screen* screen) {
    if (rewind == NULL || screen == NULL) { return; }
    rewind->frameCount++;
    if (rewind->frameCount % REWIND_THUMBNAIL_INTERVAL != 0) { return; }

    // Take a thumbnail in an unused slot, or else the oldest one. After a rewind, the thumbnails that were cut off are
    // unused, and should be replaced before any that can still be rewound to
    struct RewindThumbnail* thumbnail = &rewind->thumbnails[0];
    for (int i = 0; i < REWIND_MAX_THUMBNAILS; i++) {
        struct RewindThumbnail* candidate = &rewind->thumbnails[i];
        if (!candidate->valid) {
            thumbnail = candidate;
            break;
        }
        if (candidate->frame < thumbnail->frame) { thumbnail = candidate; }
    }
    size_t size = (size_t)rewind->width * rewind->height * sizeof(uint32_t);
    if (thumbnail->pixels == NULL) {
        thumbnail->pixels = (uint32_t*)malloc(size);
        if (thumbnail->pixels == NULL) { return; }
    }
    memcpy(thumbnail->pixels, screen->framebuffer, size);
    thumbnail->frame = rewind->frameCount;
    thumbnail->valid = true;
}

// Helper function: how many frames ago a thumbnail was taken
static uint64_t thumbnailFramesAgo(const struct Rewind* rewind, const struct RewindThumbnail* thumbnail) {
    return rewind->frameCount - thumbnail->frame;
}

// Helper function: whether a thumbnail's moment can be shown and rewound to. Showing frame N frames ago means loading
// the history entry from the frame before it and running one frame, so the history has to go back one further.
// This also rules out thumbnails from before the history was cleared (like when a new game is loaded), since the
// history restarts from nothing
static bool canRewindTo(const struct Rewind* rewind, const struct RewindThumbnail* thumbnail) {
    return thumbnail->valid && thumbnailFramesAgo(rewind, thumbnail) + 1 < (uint64_t)NES_getHistoryLength();
}

// Helper function: put the frame from rewind->framesAgo frames ago in the screen's framebuffer
static void showFrame(struct Rewind* rewind, struct Screen* screen, struct NES_Console* console) {
    size_t size = (size_t)rewind->width * rewind->height * sizeof(uint32_t);
    if (rewind->framesAgo == 0) {
        memcpy(screen->framebuffer, rewind->nowPixels, size);
        return;
    }

    // History entries are saved just after each frame, so the entry before the wanted frame is where that frame starts.
    // Running the console for one frame from there draws it into the framebuffer
    if (NES_loadHistoryState(console, rewind->framesAgo + 1) != 0) { return; }
    while (!console->ConnectedPPU->frame_complete) {
        NES_busTickMasterClock(console, true);
    }
    console->ConnectedPPU->frame_complete = false;
}

// Helper function: where along the strip the cursor goes for rewind->framesAgo, in NES pixels.
// Each thumbnail's slot covers the frames from its thumbnail to the next (or to now, for the last), spread evenly across it
static float cursorX(const struct Rewind* rewind) {
    uint64_t frame = rewind->frameCount - rewind->framesAgo;
    int slot = rewind->stripCount - 1;
    while (slot > 0 && rewind->thumbnails[rewind->strip[slot]].frame > frame) { slot--; }

    uint64_t slotStart = rewind->thumbnails[rewind->strip[slot]].frame;
    uint64_t slotEnd = (slot + 1 < rewind->stripCount) ? rewind->thumbnails[rewind->strip[slot + 1]].frame : rewind->frameCount;
    float fraction = (slotEnd > slotStart) ? (float)(frame - slotStart) / (float)(slotEnd - slotStart) : 1.0f;
    return rewind->stripX + slot * (rewind->slotWidth + REWIND_SLOT_GAP) + fraction * rewind->slotWidth;
}

// Helper function: move the cursor to rewind->framesAgo, and update the label to match
static void updateCursorAndLabel(struct Rewind* rewind, struct Overlay* overlay) {
    if (rewind->stripCount > 0) {
        float x = cursorX(rewind);
        float y = rewind->stripY - REWIND_PANEL_PADDING;
        overlaySetPosition(overlay, rewind->cursorOutlineId, x - REWIND_CURSOR_WIDTH / 2 - REWIND_CURSOR_OUTLINE, y - REWIND_CURSOR_OUTLINE);
        overlaySetPosition(overlay, rewind->cursorId, x - REWIND_CURSOR_WIDTH / 2, y);
    }

    char text[REWIND_LABEL_TEXT_LENGTH];
    if (rewind->framesAgo == 0) { snprintf(text, sizeof(text), "Rewind: now"); }
    else { snprintf(text, sizeof(text), "Rewind: %.1f seconds ago", rewind->framesAgo / REWIND_FRAMES_PER_SECOND); }

    // Text is turned into a texture, so only rebuild it when it changes
    if (rewind->labelId != OVERLAY_INVALID_ID && strcmp(text, rewind->labelText) == 0) { return; }
    overlayRemove(overlay, rewind->labelId);
    rewind->labelId = overlayAddText(overlay, text, 8, 8, OVERLAY_NO_EXPIRY);
    SDL_strlcpy(rewind->labelText, text, sizeof(rewind->labelText));
}

// Helper function: remove everything rewind added to the overlay
static void hideStrip(struct Rewind* rewind, struct Overlay* overlay) {
    overlayRemove(overlay, rewind->panelId);
    rewind->panelId = OVERLAY_INVALID_ID;
    for (int i = 0; i < REWIND_MAX_THUMBNAILS; i++) {
        overlayRemove(overlay, rewind->thumbnailIds[i]);
        rewind->thumbnailIds[i] = OVERLAY_INVALID_ID;
    }
    overlayRemove(overlay, rewind->cursorOutlineId);
    overlayRemove(overlay, rewind->cursorId);
    overlayRemove(overlay, rewind->labelId);
    rewind->cursorOutlineId = OVERLAY_INVALID_ID;
    rewind->cursorId = OVERLAY_INVALID_ID;
    rewind->labelId = OVERLAY_INVALID_ID;
}

// Helper function: lay out the strip and add the panel, thumbnails and cursor to the overlay
static void showStrip(struct Rewind* rewind, struct Overlay* overlay) {
    int count = rewind->stripCount;
    if (count == 0) { return; }

    float available = rewind->width - 2 * (REWIND_STRIP_MARGIN + REWIND_PANEL_PADDING);
    rewind->slotWidth = (available - (count - 1) * REWIND_SLOT_GAP) / count;
    if (rewind->slotWidth > REWIND_MAX_SLOT_WIDTH) { rewind->slotWidth = REWIND_MAX_SLOT_WIDTH; }
    rewind->slotHeight = rewind->slotWidth * rewind->height / rewind->width;
    float stripWidth = count * rewind->slotWidth + (count - 1) * REWIND_SLOT_GAP;
    rewind->stripX = (rewind->width - stripWidth) / 2;
    rewind->stripY = rewind->height - REWIND_STRIP_MARGIN - REWIND_PANEL_PADDING - rewind->slotHeight;

    rewind->panelId = overlayAddBox(overlay, rewind->stripX - REWIND_PANEL_PADDING, rewind->stripY - REWIND_PANEL_PADDING,
        stripWidth + 2 * REWIND_PANEL_PADDING, rewind->slotHeight + 2 * REWIND_PANEL_PADDING, REWIND_PANEL_COLOR, OVERLAY_NO_EXPIRY);
    overlaySetLayer(overlay, rewind->panelId, OVERLAY_LAYER_BACK);

    for (int i = 0; i < count; i++) {
        float x = rewind->stripX + i * (rewind->slotWidth + REWIND_SLOT_GAP);
        rewind->thumbnailIds[i] = overlayAddImageARGB(overlay, rewind->thumbnails[rewind->strip[i]].pixels, rewind->width, rewind->height,
            x, rewind->stripY, rewind->slotWidth, rewind->slotHeight, OVERLAY_NO_EXPIRY);
    }

    // The cursor runs the full height of the panel. The outline is added first, so it's drawn under the line
    float cursorHeight = rewind->slotHeight + 2 * REWIND_PANEL_PADDING;
    rewind->cursorOutlineId = overlayAddBox(overlay, 0, 0, REWIND_CURSOR_WIDTH + 2 * REWIND_CURSOR_OUTLINE, cursorHeight + 2 * REWIND_CURSOR_OUTLINE,
        0.0f, 0.0f, 0.0f, 1.0f, OVERLAY_NO_EXPIRY);
    rewind->cursorId = overlayAddBox(overlay, 0, 0, REWIND_CURSOR_WIDTH, cursorHeight, 1.0f, 1.0f, 1.0f, 1.0f, OVERLAY_NO_EXPIRY);
    overlaySetLayer(overlay, rewind->cursorOutlineId, OVERLAY_LAYER_FRONT);
    overlaySetLayer(overlay, rewind->cursorId, OVERLAY_LAYER_FRONT);
}

void rewindBegin(struct Rewind* rewind, struct Screen* screen, struct Overlay* overlay) {
    if (rewind == NULL || screen == NULL || rewind->active) { return; }
    rewind->active = true;
    rewind->framesAgo = 0;
    rewind->heldUpdates = 0;
    memcpy(rewind->nowPixels, screen->framebuffer, (size_t)rewind->width * rewind->height * sizeof(uint32_t));

    // Gather the thumbnails that can still be rewound to, oldest first. One taken this very frame is the same moment
    // as now, so it's left out
    rewind->stripCount = 0;
    for (int i = 0; i < REWIND_MAX_THUMBNAILS; i++) {
        const struct RewindThumbnail* thumbnail = &rewind->thumbnails[i];
        if (!canRewindTo(rewind, thumbnail) || thumbnailFramesAgo(rewind, thumbnail) == 0) { continue; }
        int position = rewind->stripCount;
        while (position > 0 && rewind->thumbnails[rewind->strip[position - 1]].frame > thumbnail->frame) {
            rewind->strip[position] = rewind->strip[position - 1];
            position--;
        }
        rewind->strip[position] = i;
        rewind->stripCount++;
    }

    // The cursor can go back as far as the oldest thumbnail
    rewind->maxFramesAgo = (rewind->stripCount > 0) ? (int)thumbnailFramesAgo(rewind, &rewind->thumbnails[rewind->strip[0]]) : 0;

    rewind->labelText[0] = '\0';
    showStrip(rewind, overlay);
    updateCursorAndLabel(rewind, overlay);
}

void rewindUpdate(struct Rewind* rewind, struct Screen* screen, struct Overlay* overlay, struct NES_Console* console, bool left, bool right) {
    if (rewind == NULL || !rewind->active) { return; }

    // Left goes further back in time, right comes forward. Holding both does nothing
    int direction = (left ? 1 : 0) - (right ? 1 : 0);
    if (direction == 0) {
        rewind->heldUpdates = 0;
        return;
    }
    int speed = (rewind->heldUpdates < REWIND_FAST_AFTER_UPDATES) ? REWIND_START_SPEED : REWIND_FAST_SPEED;
    rewind->heldUpdates++;

    int framesAgo = rewind->framesAgo + direction * speed;
    if (framesAgo < 0) { framesAgo = 0; }
    if (framesAgo > rewind->maxFramesAgo) { framesAgo = rewind->maxFramesAgo; }
    if (framesAgo == rewind->framesAgo) { return; }

    rewind->framesAgo = framesAgo;
    showFrame(rewind, screen, console);
    updateCursorAndLabel(rewind, overlay);
}

void rewindEnd(struct Rewind* rewind, struct Screen* screen, struct Overlay* overlay, struct NES_Console* console) {
    if (rewind == NULL || !rewind->active) { return; }
    rewind->active = false;
    hideStrip(rewind, overlay);

    // Showing frames ran the console from other states, so always load the chosen one, even if it's now.
    // The screen is already showing its picture
    if (rewind->framesAgo == 0) {
        NES_loadHistoryState(console, 0);
        return;
    }

    // NES_rewindHistory discards the history after the chosen frame, so the thumbnails after it go too
    NES_rewindHistory(console, rewind->framesAgo);
    rewind->frameCount -= rewind->framesAgo;
    for (int i = 0; i < REWIND_MAX_THUMBNAILS; i++) {
        if (rewind->thumbnails[i].valid && rewind->thumbnails[i].frame > rewind->frameCount) { rewind->thumbnails[i].valid = false; }
    }
}