#define _CRT_SECURE_NO_WARNINGS

#include "BatterySave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <direct.h>

#define NES_BATTERY_SAVE_FOLDER "Saves"
#define NES_BATTERY_SAVE_FILENAME_LENGTH 64

// Once the save RAM changes, the file is written after it has gone this many frames without changing again, so a
// game writing its save byte by byte over a few frames causes one file write instead of thousands
#define NES_BATTERY_SAVE_QUIET_FRAMES 60
// Some games use the save RAM as work RAM and change it every frame, so it never goes quiet. Those are written at most this often
#define NES_BATTERY_SAVE_MAX_FRAMES 300

// What the save RAM held at the end of the last frame, to spot changes by comparing against it
static uint8_t* lastRAM = NULL;
static size_t lastRAMSize = 0;
static bool unsavedChanges = false;
static int framesSinceChange = 0;
static int framesSinceSaved = 0;

// Helper function: the file a game's battery save is stored in, Saves/<checksum>.sav
static void getSavePath(const struct Cartridge* c, char* filename) {
    snprintf(filename, NES_BATTERY_SAVE_FILENAME_LENGTH, NES_BATTERY_SAVE_FOLDER "/%" PRIu32 ".sav", c->checksum);
}

// Helper function: change detection works by keeping a copy of the save RAM (lastRAM), and comparing the real RAM
// against it every frame. This takes a fresh copy of the RAM as it is right now, and clears the unsaved-changes flag and
// frame counters. Call it whenever the file and the RAM are known to match: after loading the save, after writing it,
// or when a new game is inserted. Future comparisons then only see changes made from this point on.
static void resetChangeTracking(const uint8_t* ram, size_t size) {
    if (lastRAMSize != size) {
        free(lastRAM);
        lastRAM = malloc(size);
        lastRAMSize = (lastRAM != NULL) ? size : 0;
    }
    if (lastRAM != NULL) { memcpy(lastRAM, ram, size); }
    unsavedChanges = false;
    framesSinceChange = 0;
    framesSinceSaved = 0;
}

int NES_loadBatterySave(struct Cartridge* c) {
    if (c == NULL || !c->has_battery) { return 0; }

    size_t size;
    uint8_t* ram = NES_getCartSaveRAM(c, &size);
    if (ram == NULL) { return 1; }

    char filename[NES_BATTERY_SAVE_FILENAME_LENGTH];
    getSavePath(c, filename);

    // No file just means the game hasn't saved yet
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        resetChangeTracking(ram, size);
        return 0;
    }

    // A file shorter than the RAM fills what it can. The rest keeps whatever the cartridge started with
    fread(ram, 1, size, f);
    bool ok = !ferror(f);
    fclose(f);
    resetChangeTracking(ram, size);

    if (!ok) {
        printf("Failed to read battery save '%s'\n", filename);
        return 1;
    }
    return 0;
}

int NES_writeBatterySave(struct Cartridge* c) {
    if (c == NULL || !c->has_battery) { return 0; }

    size_t size;
    uint8_t* ram = NES_getCartSaveRAM(c, &size);
    if (ram == NULL) { return 1; }

    char filename[NES_BATTERY_SAVE_FILENAME_LENGTH];
    char tempFilename[NES_BATTERY_SAVE_FILENAME_LENGTH + 4];
    getSavePath(c, filename);
    snprintf(tempFilename, sizeof(tempFilename), "%s.tmp", filename);

    // fopen can't create folders, so make it first. _mkdir just fails if it already exists, which is fine
    _mkdir(NES_BATTERY_SAVE_FOLDER);

    // Write to a temporary file, and only replace the real save once that has fully succeeded, so a crash or a full
    // disk partway through can't leave a half-written save behind
    FILE* f = fopen(tempFilename, "wb");
    if (f == NULL) {
        printf("Failed to open battery save '%s' for writing\n", tempFilename);
        return 1;
    }
    bool ok = fwrite(ram, 1, size, f) == size;

    // fclose flushes whatever is still buffered, so it can fail too
    if (fclose(f) != 0) { ok = false; }
    if (ok) {
        // rename can't replace an existing file on Windows, so remove the old save first
        remove(filename);
        ok = rename(tempFilename, filename) == 0;
    }
    if (!ok) {
        printf("Failed to write battery save '%s'\n", filename);
        remove(tempFilename);
        return 1;
    }

    resetChangeTracking(ram, size);
    return 0;
}

void NES_tickBatterySave(struct Cartridge* c) {
    if (c == NULL || !c->has_battery) { return; }

    size_t size;
    uint8_t* ram = NES_getCartSaveRAM(c, &size);
    if (ram == NULL) { return; }

    // Nothing to compare against yet (or out of memory). Start tracking from here
    if (lastRAM == NULL || lastRAMSize != size) {
        resetChangeTracking(ram, size);
        return;
    }

    if (memcmp(ram, lastRAM, size) != 0) {
        memcpy(lastRAM, ram, size);
        if (!unsavedChanges) { framesSinceSaved = 0; }
        unsavedChanges = true;
        framesSinceChange = 0;
    }
    else if (unsavedChanges) {
        framesSinceChange++;
    }
    if (!unsavedChanges) { return; }

    framesSinceSaved++;
    if (framesSinceChange >= NES_BATTERY_SAVE_QUIET_FRAMES || framesSinceSaved >= NES_BATTERY_SAVE_MAX_FRAMES) {
        NES_writeBatterySave(c);
    }
}