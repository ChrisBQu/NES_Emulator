#ifndef MAPPER_1_H
#define MAPPER_1_H

#include "../Cartridge.h"

// For information on Mapper 1 (MMC1), see: https://www.nesdev.org/wiki/MMC1

struct Mapper1_State {
	uint8_t shift;		// Starts as 0b10000. When the 1 reaches bit 0, the next write completes the register
	uint8_t control;	// Bits 0-1: mirroring, Bits 2-3: PRG bank mode, Bit 4: CHR bank mode
	uint8_t chr_bank_0;
	uint8_t chr_bank_1;
	uint8_t prg_bank;	// Bits 0-3: PRG bank, Bit 4: PRG RAM disable
	uint64_t last_write_cycle;
};

bool Mapper1_init(struct Cartridge* c);
uint8_t Mapper1_readPRG_ROM(struct Cartridge* c, uint16_t address);
void Mapper1_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
uint8_t Mapper1_readPRG_RAM(struct Cartridge* c, uint16_t address);
void Mapper1_writePRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data);
uint8_t Mapper1_readCHR_ROM(struct Cartridge* c, uint16_t address);
void Mapper1_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data);

#endif