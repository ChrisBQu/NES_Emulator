#ifndef BATTERYSAVE_H
#define BATTERYSAVE_H

#include "Cartridge.h"

// Battery saves are the cartridge RAM a battery keeps powered while the NES is off, which is where games keep their own
// save files. They're stored in Saves/<checksum>.sav as raw bytes, the same format other emulators use. 

// Fill a cartridge's save RAM from its .sav file. Call after the cartridge is created, before the game starts running.
// Returns 0 on success, including when the game has no battery or no save yet
int NES_loadBatterySave(struct Cartridge* c);

// Write a cartridge's save RAM to its .sav file. Call before the cartridge is freed. Returns 0 on success, including
// when the game has no battery
int NES_writeBatterySave(struct Cartridge* c);

// Call once per frame. Watches the save RAM, and writes the .sav file once the game has finished changing it, so saves
// reach the disk soon after the game makes them instead of only when the cartridge is removed
void NES_tickBatterySave(struct Cartridge* c);

#endif