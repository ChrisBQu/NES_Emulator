#define _CRT_SECURE_NO_WARNINGS

#include "Savestate.h"
#include "6502.h"
#include "PPU.h"
#include "APU.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <direct.h>

#define NES_SAVESTATE_FOLDER "Savestates"
#define NES_SAVESTATE_FOLDER_LENGTH 64
#define NES_SAVESTATE_FILENAME_LENGTH 96

// This will be a ring buffer of savestates, so that the history can be rewound to a previous state
static struct NES_Savestate* save_state_history[SAVE_STATE_HISTORY_SIZE];
static int save_state_history_cursor = -1; 
static int save_state_history_count = 0;

// Helper function: the folder and file a slot is stored in, Savestates/<checksum>/<slot>.savestate
// Slots are numbered from 1 in the file names
static void getSlotPaths(const struct Cartridge* c, int slot, char* folder, char* filename) {
    snprintf(folder, NES_SAVESTATE_FOLDER_LENGTH, NES_SAVESTATE_FOLDER "/%" PRIu32, c->checksum);
    snprintf(filename, NES_SAVESTATE_FILENAME_LENGTH, "%s/%d.savestate", folder, slot + 1);
}

int NES_saveSavestateSlot(struct NES_Console* console, int slot) {
    if (slot < 0 || slot >= NES_SAVESTATE_SLOT_COUNT) {
        printf("Invalid savestateslot number\n");
        return 1;
    }
    // Slots are kept in a folder named after the game, so there has to be a game running
    if (console->ConnectedCartridge == NULL) {
        printf("Load a game before saving a savestate\n");
        return 1;
    }

    char folder[NES_SAVESTATE_FOLDER_LENGTH];
    char filename[NES_SAVESTATE_FILENAME_LENGTH];
    getSlotPaths(console->ConnectedCartridge, slot, folder, filename);

    // fopen can't create folders, so make them first. _mkdir just fails if they already exist, which is fine
    _mkdir(NES_SAVESTATE_FOLDER);
    _mkdir(folder);

    return NES_saveSavestateToFile(console, filename);
}

// Helper functions: copy the parts of a 64KB memory array the bus uses into a savestate, and back
static void keepUsedMemory(struct NES_Savestate* savestate, const uint8_t* memory) {
    memcpy(savestate->RAM, memory, NES_SAVESTATE_RAM_SIZE);
    memcpy(savestate->IOArea, memory + NES_SAVESTATE_IO_START, NES_SAVESTATE_IO_SIZE);
}
static void restoreUsedMemory(const struct NES_Savestate* savestate, uint8_t* memory) {
    memcpy(memory, savestate->RAM, NES_SAVESTATE_RAM_SIZE);
    memcpy(memory + NES_SAVESTATE_IO_START, savestate->IOArea, NES_SAVESTATE_IO_SIZE);
}

// Makes an in-memory savestate. The rewind history is built from these
struct NES_Savestate* NES_createSavestate(struct NES_Console* console) {
    struct NES_Savestate* savestate = malloc(sizeof(struct NES_Savestate));
    if (savestate == NULL) {
        printf("Failed to allocate memory for Savestate\n");
        return NULL;
    }

    // Copy the memory state
    keepUsedMemory(savestate, console->Memory);

    // Copy the processsor state
    struct Processor* newProcessor = malloc(sizeof(struct Processor));
    if (newProcessor == NULL) {
        printf("Failed to allocate memory for Processor state copy\n");
        free(savestate);
        return NULL;
    }
    *newProcessor = *console->ConnectedProcessor;
    savestate->ProcessorBackup = newProcessor;

    // Copy the PPU state
    struct PictureProcessingUnit* newPPU = malloc(sizeof(struct PictureProcessingUnit));
    if (newPPU == NULL) {
        printf("Failed to allocate memory for PPU state copy\n");
        free(savestate);
        free(newProcessor);
        return NULL;
    }
    *newPPU = *console->ConnectedPPU;
    savestate->PPUBackup = newPPU;

    // Copy the APU state
    struct AudioProcessingUnit* newAPU = malloc(sizeof(struct AudioProcessingUnit));
    if (newAPU == NULL) {
        printf("Failed to allocate memory for APU state copy\n");
        free(savestate);
        free(newProcessor);
        free(newPPU);
        return NULL;
    }
    *newAPU = *console->ConnectedAPU;
    savestate->APUBackup = newAPU;

    // Copy the parts of the cartridge that change, not its ROM
    savestate->CartridgeBackup = NULL;
    if (console->ConnectedCartridge != NULL) {
        savestate->CartridgeBackup = NES_copyCartridgeState(console->ConnectedCartridge);
        if (savestate->CartridgeBackup == NULL) {
            printf("Failed to allocate memory for Cartridge state copy\n");
            free(savestate);
            free(newProcessor);
            free(newPPU);
            free(newAPU);
            return NULL;
        }
    }

    return savestate;
}

int NES_loadSavestateSlot(struct NES_Console* console, int slot) {
    if (slot < 0 || slot >= NES_SAVESTATE_SLOT_COUNT) {
        printf("Invalid savestateslot number\n");
        return 1;
    }
    // Slots are kept in a folder named after the game, so there has to be a game running
    if (console->ConnectedCartridge == NULL) {
        printf("Load a game before loading a savestate\n");
        return 1;
    }

    char folder[NES_SAVESTATE_FOLDER_LENGTH];
    char filename[NES_SAVESTATE_FILENAME_LENGTH];
    getSlotPaths(console->ConnectedCartridge, slot, folder, filename);

    // Reading the file only builds the savestate. Apply it to the console, then free it either way
    struct NES_Savestate* savestate = NES_loadSavestateFromFile(console, filename);
    if (savestate == NULL) { return 1; }
    int result = NES_loadSavestate(console, savestate);
    NES_freeSavestate(savestate);
    return result;
}

int NES_loadSavestate(struct NES_Console* console, struct NES_Savestate* savestate) {
    if (savestate == NULL) {
        printf("No savestate to load\n");
        return 1;
    }

    // A savestate made with no game running would otherwise remove the game that's running now
    if (savestate->CartridgeBackup == NULL && console->ConnectedCartridge != NULL) {
        printf("This savestate was made with no game loaded, so it can't be loaded over a running game\n");
        return 1;
    }

    // Restore the cartridge first, since it's the only step that can fail. If it does, the console is left untouched.
    // The savestate doesn't hold the ROM, so it's put back into the game that's running, which has to be the same game
    if (savestate->CartridgeBackup != NULL) {
        if (console->ConnectedCartridge == NULL) {
            printf("Load the game this savestate was made with first\n");
            return 1;
        }
        if (NES_restoreCartridgeState(console->ConnectedCartridge, savestate->CartridgeBackup) != 0) { return 1; }
    }

    // Restore the memory state
    restoreUsedMemory(savestate, console->Memory);

    // Restore the processor, PPU and APU states
    *console->ConnectedProcessor = *savestate->ProcessorBackup;
    *console->ConnectedPPU = *savestate->PPUBackup;
    *console->ConnectedAPU = *savestate->APUBackup;
    console->ConnectedProcessor->bus = console;
    console->ConnectedPPU->bus = console;
    console->ConnectedAPU->bus = console;

    return 0;
}

void NES_freeSavestate(struct NES_Savestate* savestate) {
    if (savestate == NULL) { return; }
    NES_freeCartridge(savestate->CartridgeBackup);
    free(savestate->ProcessorBackup);
    free(savestate->PPUBackup);
    free(savestate->APUBackup);
    free(savestate);
}

// Written at the start of every savestate file, so that files which aren't savestates, or were made by a build
// of the emulator with different structs, are rejected instead of loaded as garbage
struct SavestateFileHeader {
    char magic[4];              // Always "NESS"
    uint32_t processor_size;    // The size of each struct dumped into the file, to catch struct changes
    uint32_t ppu_size;
    uint32_t apu_size;
    uint8_t has_cartridge;
};

// Helper function: fill in the header this build of the emulator writes and expects
static void makeFileHeader(struct SavestateFileHeader* header, bool has_cartridge) {
    memset(header, 0, sizeof(struct SavestateFileHeader)); // Zero the padding bytes too, so headers can be compared with memcmp
    memcpy(header->magic, "NESS", 4);
    header->processor_size = sizeof(struct Processor);
    header->ppu_size = sizeof(struct PictureProcessingUnit);
    header->apu_size = sizeof(struct AudioProcessingUnit);
    header->has_cartridge = has_cartridge;
}

int NES_saveSavestateToFile(struct NES_Console* console, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        printf("Failed to open savestate file '%s' for writing\n", filename);
        return 1;
    }

    struct SavestateFileHeader header;
    makeFileHeader(&header, console->ConnectedCartridge != NULL);

    // The processor, PPU and APU are written as they are. Their bus pointers go along too, but those are replaced when the savestate is loaded
    bool ok = fwrite(&header, sizeof(header), 1, f) == 1
        && fwrite(console->Memory, NES_MEMORY_SIZE, 1, f) == 1
        && fwrite(console->ConnectedProcessor, sizeof(struct Processor), 1, f) == 1
        && fwrite(console->ConnectedPPU, sizeof(struct PictureProcessingUnit), 1, f) == 1
        && fwrite(console->ConnectedAPU, sizeof(struct AudioProcessingUnit), 1, f) == 1;
    if (ok && console->ConnectedCartridge != NULL) {
        ok = NES_writeCartridgeState(console->ConnectedCartridge, f) == 0;
    }

    // fclose flushes whatever is still buffered, so it can fail too
    if (fclose(f) != 0) { ok = false; }
    if (!ok) {
        printf("Failed to write savestate file '%s'\n", filename);
        remove(filename);
        return 1;
    }
    return 0;
}

struct NES_Savestate* NES_loadSavestateFromFile(struct NES_Console* console, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        printf("Failed to open savestate file '%s'\n", filename);
        return NULL;
    }

    // Check the header before reading anything else
    struct SavestateFileHeader header, expected;
    if (fread(&header, sizeof(header), 1, f) != 1 || memcmp(header.magic, "NESS", 4) != 0) {
        printf("'%s' is not a savestate file\n", filename);
        fclose(f);
        return NULL;
    }
    makeFileHeader(&expected, header.has_cartridge != 0);
    if (memcmp(&header, &expected, sizeof(header)) != 0) {
        printf("'%s' was made by a different version of the emulator, and can't be loaded\n", filename);
        fclose(f);
        return NULL;
    }

    // The file doesn't contain the game, so the game has to already be running to load its state
    if (header.has_cartridge && console->ConnectedCartridge == NULL) {
        printf("'%s' needs its game to be loaded first\n", filename);
        fclose(f);
        return NULL;
    }

    // calloc starts every pointer as NULL, so NES_freeSavestate can clean up a partly read savestate
    struct NES_Savestate* savestate = calloc(1, sizeof(struct NES_Savestate));
    if (savestate == NULL) {
        printf("Failed to allocate memory for Savestate\n");
        fclose(f);
        return NULL;
    }
    savestate->ProcessorBackup = malloc(sizeof(struct Processor));
    savestate->PPUBackup = malloc(sizeof(struct PictureProcessingUnit));
    savestate->APUBackup = malloc(sizeof(struct AudioProcessingUnit));

    // Files hold the whole 64KB memory array, so older savestate files keep working. Only the parts the bus uses are kept
    uint8_t* memory = malloc(NES_MEMORY_SIZE);
    bool ok = memory != NULL && fread(memory, NES_MEMORY_SIZE, 1, f) == 1;
    if (ok) { keepUsedMemory(savestate, memory); }
    free(memory);

    ok = ok && savestate->ProcessorBackup != NULL && savestate->PPUBackup != NULL && savestate->APUBackup != NULL
        && fread(savestate->ProcessorBackup, sizeof(struct Processor), 1, f) == 1
        && fread(savestate->PPUBackup, sizeof(struct PictureProcessingUnit), 1, f) == 1
        && fread(savestate->APUBackup, sizeof(struct AudioProcessingUnit), 1, f) == 1;
    if (ok && header.has_cartridge) {
        savestate->CartridgeBackup = NES_readCartridgeState(console->ConnectedCartridge, f);
        ok = savestate->CartridgeBackup != NULL;
    }
    fclose(f);

    if (!ok) {
        printf("Failed to read savestate file '%s'. It may be truncated or corrupted\n", filename);
        NES_freeSavestate(savestate);
        return NULL;
    }
    return savestate;
}

void NES_tickHistory(struct NES_Console* console) {
    struct NES_Savestate* state = NES_createSavestate(console);
    if (state == NULL) { return; }
    save_state_history_cursor = (save_state_history_cursor + 1) % SAVE_STATE_HISTORY_SIZE;
    NES_freeSavestate(save_state_history[save_state_history_cursor]);
    save_state_history[save_state_history_cursor] = state;
    if (save_state_history_count < SAVE_STATE_HISTORY_SIZE) { save_state_history_count++; }
}

void NES_rewindHistory(struct NES_Console* console, int frames) {
    if (save_state_history_count == 0 || frames < 0) { return; }
    if (frames > save_state_history_count - 1) { frames = save_state_history_count - 1; }
    save_state_history_cursor = (save_state_history_cursor - frames + SAVE_STATE_HISTORY_SIZE) % SAVE_STATE_HISTORY_SIZE;
    save_state_history_count -= frames;
    NES_loadSavestate(console, save_state_history[save_state_history_cursor]);
}

void NES_clearHistory(void) {
    for (int i = 0; i < SAVE_STATE_HISTORY_SIZE; i++) {
        NES_freeSavestate(save_state_history[i]);
        save_state_history[i] = NULL;
    }
    save_state_history_cursor = -1;
    save_state_history_count = 0;
}