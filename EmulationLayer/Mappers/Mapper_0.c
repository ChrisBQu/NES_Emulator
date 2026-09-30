#include "Mapper_0.h"
#include <stddef.h>

bool Mapper0_init(struct Cartridge* c) {
	// NROM has no registers
	c->mapper_state = NULL;
	return true;
}

uint8_t Mapper0_readPRG_ROM(struct Cartridge* c, uint16_t address) {
	// If there's 32 KB of prg_rom, it uses the whole address space. Otherwise, if there's 16KB, it's mirrored
	// into both $8000-$BFFF and $C000-$FFFF
	if (c->prg_rom_blocks == 2) { return c->prg_rom[(address - 0x8000) & 0x7FFF]; }
	else { return c->prg_rom[(address - 0x8000) & 0x3FFF]; }
}

void Mapper0_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {
	// NROM has no registers, so writes to ROM are ignored
}

uint8_t Mapper0_readPRG_RAM(struct Cartridge* c, uint16_t address) {
	return c->prg_ram[address & 0x1FFF];
}

void Mapper0_writePRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data) {
	c->prg_ram[address & 0x1FFF] = data;
}

uint8_t Mapper0_readCHR_ROM(struct Cartridge* c, uint16_t address) {
	return c->chr_rom[address & 0x1FFF];
}

void Mapper0_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data) {
	// Only CHR RAM is writable. Writes to CHR ROM are ignored
	if (c->chr_rom_blocks == 0) { c->chr_rom[address & 0x1FFF] = data; }
}