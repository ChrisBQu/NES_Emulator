#include "Mapper_2.h"
#include <stdio.h>
#include <stdlib.h>

#define PRG_BANK_SIZE 0x4000

bool Mapper2_init(struct Cartridge* c) {
	struct Mapper2_State* s = malloc(sizeof(struct Mapper2_State));
	if (s == NULL) {
		printf("Error: Could not create UxROM mapper state. Out of memory?\n");
		return false;
	}

	s->prg_bank = 0;
	c->mapper_state = s;
	return true;
}

uint8_t Mapper2_readPRG_ROM(struct Cartridge* c, uint16_t address) {
	struct Mapper2_State* s = c->mapper_state;

	// $8000-$BFFF is switchable, $C000-$FFFF is fixed to the last bank
	uint8_t bank = (address < 0xC000) ? (s->prg_bank % c->prg_rom_blocks) : (c->prg_rom_blocks - 1);
	return c->prg_rom[(uint32_t)bank * PRG_BANK_SIZE + (address & 0x3FFF)];
}

void Mapper2_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {
	// Any write to $8000-$FFFF selects the bank. UNROM uses 3 bits and UOROM uses 4, the modulo on read handles both
	struct Mapper2_State* s = c->mapper_state;
	s->prg_bank = data;
}

uint8_t Mapper2_readCHR_ROM(struct Cartridge* c, uint16_t address) {
	return c->chr_rom[address & 0x1FFF];
}

void Mapper2_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data) {
	// Only CHR RAM is writable. Writes to CHR ROM are ignored
	if (c->chr_rom_blocks == 0) { c->chr_rom[address & 0x1FFF] = data; }
}