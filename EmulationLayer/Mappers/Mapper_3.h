#ifndef MAPPER_3_H
#define MAPPER_3_H

#include "../Cartridge.h"

// For information on Mapper 3 (CNROM), see: https://www.nesdev.org/wiki/CNROM

struct Mapper3_State {
	uint8_t chr_bank;
};

bool Mapper3_init(struct Cartridge* c);
void Mapper3_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
uint8_t Mapper3_readCHR_ROM(struct Cartridge* c, uint16_t address);
void Mapper3_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data);

#endif
