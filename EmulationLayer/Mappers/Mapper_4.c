#include "Mapper_4.h"
#include <stdio.h>
#include <stdlib.h>

#define PRG_BANK_SIZE 0x2000
#define CHR_BANK_SIZE 0x0400

bool Mapper4_init(struct Cartridge* c) {
	struct Mapper4_State* s = malloc(sizeof(struct Mapper4_State));
	if (s == NULL) {
		printf("Error: Could not create MMC3 mapper state. Out of memory?\n");
		return false;
	}

	// The last PRG bank is always fixed at $E000, so the reset vector is reachable regardless of the power on state
	s->bank_select = 0;
	for (int i = 0; i < 8; i++) { s->banks[i] = 0; }
	s->banks[7] = 1;
	s->irq_latch = 0;
	s->irq_counter = 0;
	s->irq_reload = false;
	s->irq_enabled = false;
	s->a12_high = false;
	s->a12_low_since = 0;
	c->mapper_state = s;
	return true;
}

// Translate a CPU address ($8000-$FFFF) into an offset into prg_rom, using the current PRG bank mode
static uint32_t Mapper4_PRGOffset(struct Cartridge* c, uint16_t address) {
	struct Mapper4_State* s = c->mapper_state;
	uint16_t bank_count = (uint16_t)c->prg_rom_blocks * 2;
	uint16_t second_last = bank_count - 2;
	uint16_t bank;

	switch ((address >> 13) & 0x03) {
		// $8000-$9FFF: R6, or the second last bank in PRG mode 1
		case 0: bank = (s->bank_select & 0x40) ? second_last : (s->banks[6] & 0x3F); break;
		// $A000-$BFFF: always R7
		case 1: bank = s->banks[7] & 0x3F; break;
		// $C000-$DFFF: the second last bank, or R6 in PRG mode 1
		case 2: bank = (s->bank_select & 0x40) ? (s->banks[6] & 0x3F) : second_last; break;
		// $E000-$FFFF: always the last bank
		default: bank = bank_count - 1; break;
	}

	bank %= bank_count;
	return (uint32_t)bank * PRG_BANK_SIZE + (address & 0x1FFF);
}

// Translate a PPU address ($0000-$1FFF) into an offset into chr_rom, using the current CHR bank mode
static uint32_t Mapper4_CHROffset(struct Cartridge* c, uint16_t address) {
	struct Mapper4_State* s = c->mapper_state;
	uint16_t bank_count = (uint16_t)(c->chr_rom_blocks == 0 ? 1 : c->chr_rom_blocks) * 8;
	uint16_t bank;

	address &= 0x1FFF;

	// CHR A12 inversion swaps the 2KB banks and the 1KB banks between the two pattern tables
	uint16_t slot_address = (s->bank_select & 0x80) ? (address ^ 0x1000) : address;
	uint8_t slot = slot_address >> 10;

	// $0000-$0FFF is two 2KB banks (R0, R1), which ignore the low bit. $1000-$1FFF is four 1KB banks (R2-R5)
	if (slot < 4) { bank = (s->banks[slot >> 1] & 0xFE) | (slot & 0x01); }
	else { bank = s->banks[slot - 2]; }

	bank %= bank_count;
	return (uint32_t)bank * CHR_BANK_SIZE + (address & 0x03FF);
}

uint8_t Mapper4_readPRG_ROM(struct Cartridge* c, uint16_t address) {
	return c->prg_rom[Mapper4_PRGOffset(c, address)];
}

void Mapper4_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {
	struct Mapper4_State* s = c->mapper_state;

	// Each 8KB range holds two registers, one at even addresses and one at odd addresses
	bool odd = (address & 0x01) != 0;

	switch ((address >> 13) & 0x03) {
		case 0:
			// $8000: bank select, $8001: bank data for the register chosen by bank select
			if (!odd) { s->bank_select = data; }
			else { s->banks[s->bank_select & 0x07] = data; }
			break;
		case 1:
			// $A000: mirroring. $A001 is PRG RAM protect, which is ignored because MMC6 boards share this mapper number
			// and use that register differently, and games don't rely on the protection
			if (!odd) { c->nametable_mirroring = (data & 0x01) ? HORIZONTAL_MAPPING : VERTICAL_MAPPING; }
			break;
		case 2:
			// $C000: IRQ latch, $C001: reload the counter from the latch on the next scanline
			if (!odd) { s->irq_latch = data; }
			else {
				s->irq_counter = 0;
				s->irq_reload = true;
			}
			break;
		case 3:
			// $E000: disable the IRQ and acknowledge any pending one, $E001: enable the IRQ
			if (!odd) {
				s->irq_enabled = false;
				c->irq_asserted = false;
			}
			else { s->irq_enabled = true; }
			break;
	}
}

uint8_t Mapper4_readCHR_ROM(struct Cartridge* c, uint16_t address) {
	return c->chr_rom[Mapper4_CHROffset(c, address)];
}

void Mapper4_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data) {
	// Only CHR RAM is writable. Writes to CHR ROM are ignored
	if (c->chr_rom_blocks == 0) { c->chr_rom[Mapper4_CHROffset(c, address)] = data; }
}

static void Mapper4_tickCounter(struct Cartridge* c) {
	struct Mapper4_State* s = c->mapper_state;

	if (s->irq_counter == 0 || s->irq_reload) {
		s->irq_counter = s->irq_latch;
		s->irq_reload = false;
	}
	else { s->irq_counter--; }

	if (s->irq_counter == 0 && s->irq_enabled) { c->irq_asserted = true; }
}

// The counter is ticked by PPU A12 rising. When the background and sprites use different pattern tables, A12 rises once per scanline
// as the PPU moves between them. It also toggles within a table switch because of the nametable fetches in between pattern fetches,
// so MMC3 ignores a rise unless A12 has been low for a few CPU cycles
void Mapper4_notifyPPUAddress(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle) {
	struct Mapper4_State* s = c->mapper_state;
	bool a12 = (address & 0x1000) != 0;

	if (a12 && !s->a12_high) {
		if (cpu_cycle - s->a12_low_since >= 3) { Mapper4_tickCounter(c); }
	}
	else if (!a12 && s->a12_high) { s->a12_low_since = cpu_cycle; }

	s->a12_high = a12;
}
