#ifndef REWIND_H
#define REWIND_H

#include <stdbool.h>
#include <stdint.h>
#include "../EmulationLayer/Savestate.h"

// Declared here so the functions below can take pointers to them without including their headers
struct Screen;
struct Overlay;

// A thumbnail of the game is kept every this many frames (about 3 seconds)
#define REWIND_THUMBNAIL_INTERVAL 180

// Enough thumbnails to cover the whole rewind history
#define REWIND_MAX_THUMBNAILS (SAVE_STATE_HISTORY_SIZE / REWIND_THUMBNAIL_INTERVAL + 1)

#define REWIND_LABEL_TEXT_LENGTH 64

struct RewindThumbnail {
    bool valid;
    uint64_t frame;     // frameCount when it was taken, which matches the history entry made on the same frame
    uint32_t* pixels;   // A copy of the screen's framebuffer (ARGB8888)
};

struct Rewind {
    int width;
    int height;

    // Counts the frames added to the rewind history, so thumbnails can be matched to history entries
    uint64_t frameCount;
    // In no particular order. When they're all in use, the oldest is replaced
    struct RewindThumbnail thumbnails[REWIND_MAX_THUMBNAILS];

    // While rewind is held
    bool active;
    // The thumbnails on the strip, oldest first (indexes into thumbnails). Each one stands for the frames from when it
    // was taken up to when the next one was, and the last one for the frames up to now
    int strip[REWIND_MAX_THUMBNAILS];
    int stripCount;
    // How far back to rewind, in frames, and the furthest it can go. 0 is the moment rewind was pressed
    int framesAgo;
    int maxFramesAgo;
    // The picture when rewind was pressed, shown again when the cursor goes back to now
    uint32_t* nowPixels;
    // How many updates in a row left or right has been held. The cursor speeds up the longer it's held
    int heldUpdates;

    // Where the strip's thumbnails are, in NES pixels
    float stripX;
    float stripY;
    float slotWidth;
    float slotHeight;

    // The overlay elements: the panel behind the strip, the thumbnails, the cursor (a dark outline and a white line), and the label
    int panelId;
    int thumbnailIds[REWIND_MAX_THUMBNAILS];
    int cursorOutlineId;
    int cursorId;
    int labelId;
    char labelText[REWIND_LABEL_TEXT_LENGTH];
};

struct Rewind* createRewind(int width, int height);
void destroyRewind(struct Rewind* rewind);

// Call once for every frame the NES runs, after NES_tickHistory, while the frame is still in the screen's framebuffer
void rewindTickFrame(struct Rewind* rewind, struct Screen* screen);

// Call when the rewind key is pressed. Pauses on the current moment and shows the thumbnail strip
void rewindBegin(struct Rewind* rewind, struct Screen* screen, struct Overlay* overlay);

// Call every frame while rewind is held, with whether left and right are held. Left moves the cursor back in time and
// right forward, faster the longer they're held. The screen shows the frame the cursor is on, which runs the console
// for a frame, so the console's state is only meaningful again after rewindEnd
void rewindUpdate(struct Rewind* rewind, struct Screen* screen, struct Overlay* overlay, struct NES_Console* console, bool left, bool right);

// Call when the rewind key is released. Loads the frame the cursor is on into the console, discards the history after
// it, and hides the strip
void rewindEnd(struct Rewind* rewind, struct Screen* screen, struct Overlay* overlay, struct NES_Console* console);

#endif