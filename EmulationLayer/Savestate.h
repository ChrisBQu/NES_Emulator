#ifndef SAVESTATE_H
#define SAVESTATE_H

#include "Bus.h"

#define NES_SAVESTATE_SLOT_COUNT 10

struct NES_Savestate {
    uint8_t Memory[NES_MEMORY_SIZE];
    struct Cartridge* CartridgeBackup;
	struct Processor* ProcessorBackup;
	struct PictureProcessingUnit* PPUBackup;
	struct AudioProcessingUnit* APUBackup;
};

// Save and load savestates to and from a file
int NES_saveSavestateToFile(struct NES_Console* console, const char* filename);

// The file only holds the game's RAM and mapper state, not the game itself, so the same game must be running on console.
// Returns NULL if it isn't. Pass the result to NES_loadSavestate, then free it with NES_freeSavestate
struct NES_Savestate* NES_loadSavestateFromFile(struct NES_Console* console, const char* filename);

// Save and load savestates from the object
int NES_loadSavestate(struct NES_Console* console, struct NES_Savestate* savestate);

// Save and load savestatess using the built-in slots
int NES_loadSavestateSlot(struct NES_Console* console, int slot);
int NES_saveSavestateSlot(struct NES_Console* console, int slot);

// Free the memory allocated for the savestate
void NES_freeSavestate(struct NES_Savestate* savestate);

#endif