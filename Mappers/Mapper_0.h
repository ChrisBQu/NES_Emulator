#ifndef MAPPER_0_H
#define MAPPER_0_H

#include "../EmulationLayer/Cartridge.h"

bool Mapper0_init(struct Cartridge* c);
uint8_t Mapper0_readPRG_ROM(struct Cartridge* c, uint16_t address);
void Mapper0_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
uint8_t Mapper0_readPRG_RAM(struct Cartridge* c, uint16_t address);
void Mapper0_writePRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data);
uint8_t Mapper0_readCHR_ROM(struct Cartridge* c, uint16_t address);
void Mapper0_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data);

#endif