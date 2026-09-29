#ifndef MAPPER_2_H
#define MAPPER_2_H

#include "../EmulationLayer/Cartridge.h"

// For information on Mapper 2 (UxROM), see: https://www.nesdev.org/wiki/UxROM

struct Mapper2_State {
	uint8_t prg_bank;
};

bool Mapper2_init(struct Cartridge* c);
uint8_t Mapper2_readPRG_ROM(struct Cartridge* c, uint16_t address);
void Mapper2_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
uint8_t Mapper2_readCHR_ROM(struct Cartridge* c, uint16_t address);
void Mapper2_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data);

#endif