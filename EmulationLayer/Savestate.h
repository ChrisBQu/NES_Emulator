#ifndef SAVESTATE_H
#define SAVESTATE_H

#include "Bus.h"

#define NES_SAVESTATE_SLOT_COUNT 10

#define SAVE_STATE_HISTORY_SIZE 1800

// The parts of the console's Memory array the bus actually uses: the 2KB of CPU RAM, and $4000-$401F, where accesses
// the bus doesn't hand to the APU or controllers land. The rest of the array is never touched, so it isn't kept
#define NES_SAVESTATE_RAM_SIZE 0x800
#define NES_SAVESTATE_IO_START 0x4000
#define NES_SAVESTATE_IO_SIZE 0x20

struct NES_Savestate {
    uint8_t RAM[NES_SAVESTATE_RAM_SIZE];
    uint8_t IOArea[NES_SAVESTATE_IO_SIZE];
    // Only the parts of the cartridge that change as the game runs (see NES_copyCartridgeState). The ROM comes from
    // the running game when the state is loaded
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

// Save the cuurrent state of the console to the history
void NES_tickHistory(struct NES_Console* console);

// Rewind the console to the state from this many frames ago. Everything in the history after it is discarded
void NES_rewindHistory(struct NES_Console* console, int frames);

// Load the state from this many frames ago, but keep the history as it is, so it can still go forward again.
// Returns 0 on success, or 1 if the history doesn't go back that far
int NES_loadHistoryState(struct NES_Console* console, int frames);

// How many frames of history there are. NES_rewindHistory can go back at most one less than this
int NES_getHistoryLength(void);

// Free every state in the history. Call this when a new game is loaded or the console is reset
void NES_clearHistory(void);

#endif