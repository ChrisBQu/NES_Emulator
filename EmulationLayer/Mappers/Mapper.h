#ifndef MAPPER_H
#define MAPPER_H

#include "../Cartridge.h"
#include <stddef.h>

#include "Mapper_0.h"
#include "Mapper_1.h"
#include "Mapper_2.h"
#include "Mapper_3.h"
#include "Mapper_4.h"
#include "Mapper_5.h"

// Mappers are used to handle the different types of cartridges that can be used with the NES
// Each mapper contains a set of functions defining how the cartridge is read from and written to
struct Mapper {
	size_t state_size;	// Size of the struct init puts in mapper_state, 0 for mappers without one
	bool (*init)(struct Cartridge* c);
	uint8_t (*readPRG_ROM)(struct Cartridge* c, uint16_t address);
	void (*writePRG)(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
	uint8_t (*readPRG_RAM)(struct Cartridge* c, uint16_t address);
	void (*writePRG_RAM)(struct Cartridge* c, uint16_t address, uint8_t data);
	uint8_t (*readCHR_ROM)(struct Cartridge* c, uint16_t address);
	void (*writeCHR)(struct Cartridge* c, uint16_t address, uint8_t data);

	// The rest are optional, and NULL for mappers that don't need them
	// This is where things get really complicated, but MMC5 needs them.

	// Sees every address on the PPU bus
	void (*notifyPPUAddress)(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle);

	// Sees every CPU write to the PPU registers ($2000-$3FFF)
	void (*notifyPPURegisterWrite)(struct Cartridge* c, uint16_t address, uint8_t data);

	// Registers or RAM in the expansion area ($4020-$5FFF)
	uint8_t (*readExpansion)(struct Cartridge* c, uint16_t address);
	void (*writeExpansion)(struct Cartridge* c, uint16_t address, uint8_t data);

	// Nametable accesses, for mappers that route nametables somewhere other than the console's 2KB of CIRAM.
	// offset is 0x000-0xFFF (the four nametables), and ciram is the console's nametable RAM
	uint8_t (*readNametable)(struct Cartridge* c, uint16_t offset, uint8_t* ciram);
	void (*writeNametable)(struct Cartridge* c, uint16_t offset, uint8_t* ciram, uint8_t data);

	// Ticked once per CPU cycle
	void (*tick)(struct Cartridge* c);

	// The RAM a battery keeps powered, for mappers that keep their PRG RAM somewhere other than the cartridge's prg_ram.
	// Sets size to its length in bytes
	uint8_t* (*getSaveRAM)(struct Cartridge* c, size_t* size);
};

#define MAPPER_COUNT 6

extern struct Mapper MapperList[MAPPER_COUNT];

#endif