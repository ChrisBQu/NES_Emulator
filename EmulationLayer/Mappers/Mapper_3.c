#include "Mapper_3.h"
#include "Mapper_0.h"
#include <stdio.h>
#include <stdlib.h>

#define CHR_BANK_SIZE 0x2000

bool Mapper3_init(struct Cartridge* c) {
	struct Mapper3_State* s = malloc(sizeof(struct Mapper3_State));
	if (s == NULL) {
		printf("Error: Could not create CNROM mapper state. Out of memory?\n");
		return false;
	}

	s->chr_bank = 0;
	c->mapper_state = s;
	return true;
}

// Translate a PPU address ($0000-$1FFF) into an offset into chr_rom, using the selected 8KB bank
static uint32_t Mapper3_CHROffset(struct Cartridge* c, uint16_t address) {
	struct Mapper3_State* s = c->mapper_state;
	uint8_t bank_count = (c->chr_rom_blocks == 0) ? 1 : c->chr_rom_blocks;
	uint8_t bank = s->chr_bank % bank_count;
	return (uint32_t)bank * CHR_BANK_SIZE + (address & 0x1FFF);
}

void Mapper3_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {
	// Any write to $8000-$FFFF selects the 8KB CHR bank. Boards use 2 bits officially, the modulo on read handles oversized ones
	// CNROM has bus conflicts: the ROM drives the data bus at the same time as the CPU, so the value latched is the AND of both
	struct Mapper3_State* s = c->mapper_state;
	s->chr_bank = data & Mapper0_readPRG_ROM(c, address);
}

uint8_t Mapper3_readCHR_ROM(struct Cartridge* c, uint16_t address) {
	return c->chr_rom[Mapper3_CHROffset(c, address)];
}

void Mapper3_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data) {
	// Only CHR RAM is writable. Writes to CHR ROM are ignored
	if (c->chr_rom_blocks == 0) { c->chr_rom[Mapper3_CHROffset(c, address)] = data; }
}
