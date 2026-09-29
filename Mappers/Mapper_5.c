#include "Mapper_5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRG_BANK_SIZE 0x2000
#define NO_ADDRESS 0xFFFF

bool Mapper5_init(struct Cartridge* c) {
	struct Mapper5_State* s = calloc(1, sizeof(struct Mapper5_State));
	if (s == NULL) {
		printf("Error: Could not create MMC5 mapper state. Out of memory?\n");
		return false;
	}

	// At power on, every ROM bank register points at the last bank so the reset vector is reachable
	s->prg_mode = 3;
	s->chr_mode = 3;
	memset(&s->prg_banks[1], 0xFF, 4);
	s->multiplicand = 0xFF;
	s->multiplier = 0xFF;
	s->last_name_address = NO_ADDRESS;
	c->mapper_state = s;
	return true;
}

static void Mapper5_updateIRQ(struct Cartridge* c) {
	struct Mapper5_State* s = c->mapper_state;
	c->irq_asserted = s->irq_pending && s->irq_enabled;
}

// Find the byte of PRG ROM or PRG RAM that a CPU address ($6000-$FFFF) maps to, using the current PRG mode
static uint8_t* Mapper5_PRGByte(struct Cartridge* c, uint16_t address, bool* is_rom) {
	struct Mapper5_State* s = c->mapper_state;
	uint16_t size = 0x2000;
	uint8_t reg;

	if (address < 0x8000) { reg = s->prg_banks[0]; }
	else {
		switch (s->prg_mode) {
			case 0:
				reg = s->prg_banks[4];
				size = 0x8000;
				break;
			case 1:
				reg = (address < 0xC000) ? s->prg_banks[2] : s->prg_banks[4];
				size = 0x4000;
				break;
			case 2:
				if (address < 0xC000) {
					reg = s->prg_banks[2];
					size = 0x4000;
				}
				else { reg = (address < 0xE000) ? s->prg_banks[3] : s->prg_banks[4]; }
				break;
			default:
				reg = s->prg_banks[1 + ((address - 0x8000) >> 13)];
				break;
		}
	}

	// Bank numbers are always in 8KB units. Larger banks ignore the low bits of the bank number
	uint32_t bank = (reg & 0x7F) & ~((uint32_t)(size / PRG_BANK_SIZE) - 1);
	uint32_t offset = bank * PRG_BANK_SIZE + (address & (size - 1));

	*is_rom = (reg & 0x80) != 0;
	if (*is_rom) { return &c->prg_rom[offset % ((uint32_t)c->prg_rom_blocks * 0x4000)]; }
	return &s->prg_ram[offset & 0xFFFF];
}

static uint8_t Mapper5_readPRGByte(struct Cartridge* c, uint16_t address) {
	bool is_rom;
	return *Mapper5_PRGByte(c, address, &is_rom);
}

static void Mapper5_writePRGByte(struct Cartridge* c, uint16_t address, uint8_t data) {
	struct Mapper5_State* s = c->mapper_state;
	bool is_rom;
	uint8_t* byte = Mapper5_PRGByte(c, address, &is_rom);
	if (!is_rom && s->prg_ram_protect[0] == 0x02 && s->prg_ram_protect[1] == 0x01) { *byte = data; }
}

uint8_t Mapper5_readPRG_ROM(struct Cartridge* c, uint16_t address) {
	return Mapper5_readPRGByte(c, address);
}

void Mapper5_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {
	// MMC5 has no registers in $8000-$FFFF, but PRG RAM can be banked into $8000-$DFFF
	Mapper5_writePRGByte(c, address, data);
}

uint8_t Mapper5_readPRG_RAM(struct Cartridge* c, uint16_t address) {
	return Mapper5_readPRGByte(c, address);
}

void Mapper5_writePRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data) {
	Mapper5_writePRGByte(c, address, data);
}

// Translate a PPU address ($0000-$1FFF) into an offset into chr_rom, using the current CHR mode and bank set
static uint32_t Mapper5_CHROffset(struct Cartridge* c, uint16_t address) {
	struct Mapper5_State* s = c->mapper_state;
	uint32_t chr_size = (uint32_t)(c->chr_rom_blocks == 0 ? 1 : c->chr_rom_blocks) * 0x2000;

	address &= 0x1FFF;

	// Extended attribute mode: each background tile picks its own 4KB bank from its ExRAM byte
	if (s->exram_mode == 1 && s->bg_pattern_fetch) {
		uint32_t bank = (s->ext_attribute & 0x3F) | ((uint32_t)s->chr_upper << 6);
		return (bank * 0x1000 + (address & 0x0FFF)) % chr_size;
	}

	// With 8x16 sprites, set A is used for sprites and set B for the background. When the PPU isn't rendering (the CPU is
	// accessing CHR through $2007), whichever set was written last is used. With 8x8 sprites, set A is used for everything
	bool use_set_a;
	if (!s->large_sprites) { use_set_a = true; }
	else if (s->in_frame) { use_set_a = !s->bg_pattern_fetch; }
	else { use_set_a = !s->last_chr_set_b; }

	// Set B only has four registers, which cover $0000-$0FFF and repeat for $1000-$1FFF
	uint32_t size;
	uint16_t bank;
	switch (s->chr_mode) {
		case 0:
			size = 0x2000;
			bank = use_set_a ? s->chr_banks_a[7] : s->chr_banks_b[3];
			break;
		case 1:
			size = 0x1000;
			bank = use_set_a ? s->chr_banks_a[(address >> 12) * 4 + 3] : s->chr_banks_b[3];
			break;
		case 2:
			size = 0x0800;
			bank = use_set_a ? s->chr_banks_a[(address >> 11) * 2 + 1] : s->chr_banks_b[((address >> 11) & 0x01) * 2 + 1];
			break;
		default:
			size = 0x0400;
			bank = use_set_a ? s->chr_banks_a[address >> 10] : s->chr_banks_b[(address >> 10) & 0x03];
			break;
	}

	return ((uint32_t)bank * size + (address & (size - 1))) % chr_size;
}

uint8_t Mapper5_readCHR_ROM(struct Cartridge* c, uint16_t address) {
	return c->chr_rom[Mapper5_CHROffset(c, address)];
}

void Mapper5_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data) {
	// Only CHR RAM is writable. Writes to CHR ROM are ignored
	if (c->chr_rom_blocks == 0) { c->chr_rom[Mapper5_CHROffset(c, address)] = data; }
}

// Called when the MMC5 sees a new scanline start. The first one after the PPU has been idle starts a new frame
static void Mapper5_startScanline(struct Cartridge* c) {
	struct Mapper5_State* s = c->mapper_state;

	if (!s->in_frame) {
		s->in_frame = true;
		s->scanline = 0;
		s->irq_pending = false;
	}
	else {
		s->scanline++;
		if (s->scanline == s->irq_scanline) { s->irq_pending = true; }
	}
	Mapper5_updateIRQ(c);
}

// The MMC5 has no idea where the PPU is, so it works it out from the addresses the PPU reads:
//  - Each scanline starts with the same nametable byte being read three times in a row (two dummy fetches at the end of the
//    previous line, then the first fetch of the new one)
//  - A background tile is a nametable byte, an attribute byte, then two pattern bytes. A sprite fetch is two (garbage)
//    nametable bytes, then two pattern bytes
void Mapper5_notifyPPUAddress(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle) {
	struct Mapper5_State* s = c->mapper_state;
	bool nametable = address >= 0x2000 && address < 0x3F00;
	bool attribute = nametable && (address & 0x03FF) >= 0x03C0;
	bool name = nametable && !attribute;

	s->ppu_idle_cycles = 0;

	// Scanline detection
	if (name && address == s->last_name_address) {
		s->name_repeats++;
		if (s->name_repeats == 2) { Mapper5_startScanline(c); }
	}
	else { s->name_repeats = 0; }

	// Track which part of a tile fetch this is
	bool previous_was_name = s->last_name_address != NO_ADDRESS;
	s->bg_attribute_fetch = attribute && previous_was_name;
	s->bg_pattern_fetch = false;
	if (name) { s->ext_attribute = s->exram[address & 0x03FF]; }
	if (attribute) { s->bg_pattern_fetches = previous_was_name ? 2 : 0; }
	else if (name) { s->bg_pattern_fetches = 0; }
	else if (address < 0x2000 && s->bg_pattern_fetches > 0) {
		s->bg_pattern_fetch = true;
		s->bg_pattern_fetches--;
	}

	s->last_name_address = name ? address : NO_ADDRESS;
}

void Mapper5_notifyPPURegisterWrite(struct Cartridge* c, uint16_t address, uint8_t data) {
	struct Mapper5_State* s = c->mapper_state;
	if ((address & 0x07) == 0) { s->large_sprites = (data & 0x20) != 0; }
}

uint8_t Mapper5_readExpansion(struct Cartridge* c, uint16_t address) {
	struct Mapper5_State* s = c->mapper_state;

	// ExRAM is only readable by the CPU in modes 2 and 3
	if (address >= 0x5C00) {
		if (s->exram_mode >= 2) { return s->exram[address - 0x5C00]; }
		return address >> 8;
	}

	switch (address) {
		case 0x5204: {
			// IRQ status. Reading acknowledges the IRQ
			uint8_t status = (s->irq_pending ? 0x80 : 0x00) | (s->in_frame ? 0x40 : 0x00);
			s->irq_pending = false;
			Mapper5_updateIRQ(c);
			return status;
		}
		case 0x5205: return (uint8_t)(s->multiplicand * s->multiplier);
		case 0x5206: return (uint8_t)((s->multiplicand * s->multiplier) >> 8);
	}

	// Anything else is open bus. The last byte on the bus is usually the high byte of the address
	return address >> 8;
}

void Mapper5_writeExpansion(struct Cartridge* c, uint16_t address, uint8_t data) {
	struct Mapper5_State* s = c->mapper_state;

	if (address >= 0x5C00) {
		// In modes 0 and 1, ExRAM belongs to the PPU. The CPU can only write it while the PPU is rendering, and
		// writes at other times store 0. Mode 3 is read-only
		if (s->exram_mode <= 1) { s->exram[address - 0x5C00] = s->in_frame ? data : 0; }
		else if (s->exram_mode == 2) { s->exram[address - 0x5C00] = data; }
		return;
	}

	if (address >= 0x5113 && address <= 0x5117) {
		if (address == 0x5113) { data &= 0x7F; }
		if (address == 0x5117) { data |= 0x80; }
		s->prg_banks[address - 0x5113] = data;
	}
	else if (address >= 0x5120 && address <= 0x5127) {
		s->chr_banks_a[address - 0x5120] = data | ((uint16_t)s->chr_upper << 8);
		s->last_chr_set_b = false;
	}
	else if (address >= 0x5128 && address <= 0x512B) {
		s->chr_banks_b[address - 0x5128] = data | ((uint16_t)s->chr_upper << 8);
		s->last_chr_set_b = true;
	}
	else {
		switch (address) {
			case 0x5100: s->prg_mode = data & 0x03; break;
			case 0x5101: s->chr_mode = data & 0x03; break;
			case 0x5102: s->prg_ram_protect[0] = data & 0x03; break;
			case 0x5103: s->prg_ram_protect[1] = data & 0x03; break;
			case 0x5104: s->exram_mode = data & 0x03; break;
			case 0x5105: s->nametable_mapping = data; break;
			case 0x5106: s->fill_tile = data; break;
			case 0x5107: s->fill_color = data & 0x03; break;
			case 0x5130: s->chr_upper = data & 0x03; break;
			case 0x5203: s->irq_scanline = data; break;
			case 0x5204:
				s->irq_enabled = (data & 0x80) != 0;
				Mapper5_updateIRQ(c);
				break;
			case 0x5205: s->multiplicand = data; break;
			case 0x5206: s->multiplier = data; break;
		}
	}
}

uint8_t Mapper5_readNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram) {
	struct Mapper5_State* s = c->mapper_state;
	uint16_t within = offset & 0x03FF;

	// Extended attribute mode: background attribute fetches return the palette from the tile's ExRAM byte,
	// repeated for all four quadrants
	if (s->exram_mode == 1 && s->bg_attribute_fetch) { return (s->ext_attribute >> 6) * 0x55; }

	switch ((s->nametable_mapping >> (((offset >> 10) & 0x03) * 2)) & 0x03) {
		case 0: return ciram[within];
		case 1: return ciram[0x400 + within];
		case 2: return (s->exram_mode <= 1) ? s->exram[within] : 0;
		default: return (within < 0x03C0) ? s->fill_tile : s->fill_color * 0x55;
	}
}

void Mapper5_writeNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram, uint8_t data) {
	struct Mapper5_State* s = c->mapper_state;
	uint16_t within = offset & 0x03FF;

	// Writes to a fill mode nametable are ignored
	switch ((s->nametable_mapping >> (((offset >> 10) & 0x03) * 2)) & 0x03) {
		case 0: ciram[within] = data; break;
		case 1: ciram[0x400 + within] = data; break;
		case 2: if (s->exram_mode <= 1) { s->exram[within] = data; } break;
	}
}

// The PPU reads memory continuously while rendering, so once it has been quiet for 3 CPU cycles the frame is over
void Mapper5_tick(struct Cartridge* c) {
	struct Mapper5_State* s = c->mapper_state;
	if (s->ppu_idle_cycles >= 3) { return; }

	s->ppu_idle_cycles++;
	if (s->ppu_idle_cycles == 3) {
		s->in_frame = false;
		s->last_name_address = NO_ADDRESS;
		s->name_repeats = 0;
	}
}