#ifndef MAPPER_4_H
#define MAPPER_4_H

#include "../Cartridge.h"

// For information on Mapper 4 (MMC3), see: https://www.nesdev.org/wiki/MMC3

struct Mapper4_State {
	uint8_t bank_select;	// Bits 0-2: which bank register the next $8001 write updates, Bit 6: PRG bank mode, Bit 7: CHR A12 inversion
	uint8_t banks[8];		// R0-R1: 2KB CHR banks, R2-R5: 1KB CHR banks, R6-R7: 8KB PRG banks
	uint8_t irq_latch;		// Value the scanline counter is reloaded with
	uint8_t irq_counter;
	bool irq_reload;		// Set by a write to $C001, reloads the counter on the next scanline
	bool irq_enabled;
	bool a12_high;			// Last seen state of PPU address line A12
	uint64_t a12_low_since;	// CPU cycle A12 last went low
};

bool Mapper4_init(struct Cartridge* c);
uint8_t Mapper4_readPRG_ROM(struct Cartridge* c, uint16_t address);
void Mapper4_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
uint8_t Mapper4_readCHR_ROM(struct Cartridge* c, uint16_t address);
void Mapper4_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data);
void Mapper4_notifyPPUAddress(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle);

#endif
