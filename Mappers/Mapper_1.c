#include "Mapper_1.h"
#include <stdio.h>
#include <stdlib.h>

#define PRG_BANK_SIZE 0x4000
#define CHR_BANK_SIZE 0x1000

bool Mapper1_init(struct Cartridge* c) {
	struct Mapper1_State* s = malloc(sizeof(struct Mapper1_State));
	if (s == NULL) {
		printf("Error: Could not create MMC1 mapper state. Out of memory?\n");
		return false;
	}

	// At power on, PRG mode 3 fixes the last bank at $C000 so the reset vector is always reachable
	s->shift = 0x10;
	s->control = 0x0C;
	s->chr_bank_0 = 0;
	s->chr_bank_1 = 0;
	s->prg_bank = 0;
	s->last_write_cycle = 0;
	c->mapper_state = s;
	return true;
}

// Translate a CPU address ($8000-$FFFF) into an offset into prg_rom, using the current PRG bank mode
static uint32_t Mapper1_PRGOffset(struct Cartridge* c, uint16_t address) {
	struct Mapper1_State* s = c->mapper_state;
	uint8_t bank = s->prg_bank & 0x0F;

	// 512KB boards (SUROM) use bit 4 of the CHR bank 0 register to select which 256KB half of PRG ROM is used
	uint8_t outer_bank = (c->prg_rom_blocks > 16) ? (s->chr_bank_0 & 0x10) : 0;

	switch ((s->control >> 2) & 0x03) {
		case 0:
		case 1:
			// Switch 32KB at $8000, ignoring the low bit of the bank number
			bank = (bank & 0x0E) | (address >= 0xC000 ? 1 : 0);
			break;
		case 2:
			// Fix the first bank at $8000, switch 16KB at $C000
			if (address < 0xC000) { bank = 0; }
			break;
		case 3:
			// Fix the last bank at $C000, switch 16KB at $8000
			if (address >= 0xC000) { bank = 0x0F; }
			break;
	}

	bank = (bank | outer_bank) % c->prg_rom_blocks;
	return (uint32_t)bank * PRG_BANK_SIZE + (address & 0x3FFF);
}

// Translate a PPU address ($0000-$1FFF) into an offset into chr_rom, using the current CHR bank mode
static uint32_t Mapper1_CHROffset(struct Cartridge* c, uint16_t address) {
	struct Mapper1_State* s = c->mapper_state;
	uint8_t bank_count = (c->chr_rom_blocks == 0 ? 1 : c->chr_rom_blocks) * 2;
	uint8_t bank;

	address &= 0x1FFF;

	// Two separate 4KB banks
	if (s->control & 0x10) { bank = (address < 0x1000) ? s->chr_bank_0 : s->chr_bank_1; }

	// One 8KB bank, ignoring the low bit of the bank number
	else { bank = (s->chr_bank_0 & 0x1E) | (address >> 12); }

	bank %= bank_count;
	return (uint32_t)bank * CHR_BANK_SIZE + (address & 0x0FFF);
}

uint8_t Mapper1_readPRG_ROM(struct Cartridge* c, uint16_t address) {
	return c->prg_rom[Mapper1_PRGOffset(c, address)];
}

void Mapper1_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {
	struct Mapper1_State* s = c->mapper_state;

	// MMC1 ignores a write on the cycle right after another write, which happens with the second write of a
	// read-modify-write instruction. 
	bool consecutive = (cpu_cycle - s->last_write_cycle) <= 1;
	s->last_write_cycle = cpu_cycle;
	if (consecutive) { return; }

	// Writing a value with bit 7 set resets the shift register, and sets PRG mode 3
	if (data & 0x80) {
		s->shift = 0x10;
		s->control |= 0x0C;
		return;
	}

	// Bits are shifted in LSB first. On the fifth write, the 1 we started with has reached bit 0
	bool complete = (s->shift & 0x01) != 0;
	s->shift = (s->shift >> 1) | ((data & 0x01) << 4);
	if (!complete) { return; }

	uint8_t value = s->shift;
	s->shift = 0x10;

	// Bits 13-14 of the address of the fifth write select which register receives the value
	switch ((address >> 13) & 0x03) {
		case 0: {
			static const SCROLL_MAPPING_TYPE mirroring[4] = {
				SINGLE_SCREEN_LOWER_MAPPING, SINGLE_SCREEN_UPPER_MAPPING, VERTICAL_MAPPING, HORIZONTAL_MAPPING
			};
			s->control = value;
			c->nametable_mirroring = mirroring[value & 0x03];
			break;
		}
		case 1: s->chr_bank_0 = value; break;
		case 2: s->chr_bank_1 = value; break;
		case 3: s->prg_bank = value; break;
	}
}

uint8_t Mapper1_readPRG_RAM(struct Cartridge* c, uint16_t address) {
	struct Mapper1_State* s = c->mapper_state;

	// When PRG RAM is disabled, reads return open bus. The last byte on the bus is usually the high byte of the address
	if (s->prg_bank & 0x10) { return address >> 8; }
	return c->prg_ram[address & 0x1FFF];
}

void Mapper1_writePRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data) {
	struct Mapper1_State* s = c->mapper_state;
	if (!(s->prg_bank & 0x10)) { c->prg_ram[address & 0x1FFF] = data; }
}

uint8_t Mapper1_readCHR_ROM(struct Cartridge* c, uint16_t address) {
	return c->chr_rom[Mapper1_CHROffset(c, address)];
}

void Mapper1_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data) {
	// Only CHR RAM is writable. Writes to CHR ROM are ignored
	if (c->chr_rom_blocks == 0) { c->chr_rom[Mapper1_CHROffset(c, address)] = data; }
}