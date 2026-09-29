#ifndef MAPPER_5_H
#define MAPPER_5_H

#include "../NF_Cartridge.h"

// For information on Mapper 5 (MMC5), see: https://www.nesdev.org/wiki/MMC5
// Not implemented: expansion audio ($5000-$5015) and vertical split mode ($5200-$5202)

struct Mapper5_State {
	// Registers
	uint8_t prg_mode;				// $5100: 0 = one 32KB bank, 1 = two 16KB, 2 = 16KB + two 8KB, 3 = four 8KB
	uint8_t chr_mode;				// $5101: 0 = 8KB banks, 1 = 4KB, 2 = 2KB, 3 = 1KB
	uint8_t prg_ram_protect[2];		// $5102, $5103: PRG RAM is only writable while these hold $02 and $01
	uint8_t exram_mode;				// $5104: 0 = nametable, 1 = extended attributes, 2 = CPU RAM, 3 = CPU read-only
	uint8_t nametable_mapping;		// $5105: 2 bits per nametable. 0/1 = CIRAM page, 2 = ExRAM, 3 = fill mode
	uint8_t fill_tile;				// $5106
	uint8_t fill_color;				// $5107
	uint8_t prg_banks[5];			// $5113-$5117. Bit 7 selects ROM (1) or RAM (0). $5113 is always RAM, $5117 is always ROM
	uint16_t chr_banks_a[8];		// $5120-$5127: sprites in 8x16 mode, everything in 8x8 mode
	uint16_t chr_banks_b[4];		// $5128-$512B: background in 8x16 mode
	uint8_t chr_upper;				// $5130: upper bits of CHR bank numbers
	bool last_chr_set_b;			// Set B was written more recently than set A
	uint8_t irq_scanline;			// $5203
	bool irq_enabled;
	bool irq_pending;
	uint8_t multiplicand;			// $5205
	uint8_t multiplier;				// $5206

	// What the MMC5 has worked out by watching the CPU and PPU buses
	bool large_sprites;				// PPUCTRL bit 5 (8x16 sprites), snooped from CPU writes to $2000
	bool in_frame;					// The PPU is rendering
	uint8_t scanline;				// Scanlines started since the frame began
	uint8_t ppu_idle_cycles;		// CPU cycles since the PPU last read anything
	uint16_t last_name_address;		// Address of the previous PPU read if it was a nametable (tile) byte, otherwise 0xFFFF
	uint8_t name_repeats;			// How many times in a row that address has been read again
	uint8_t bg_pattern_fetches;		// Background pattern fetches left for the current tile
	bool bg_pattern_fetch;			// The current PPU read is a background pattern fetch
	bool bg_attribute_fetch;		// The current PPU read is a background attribute fetch
	uint8_t ext_attribute;			// ExRAM byte for the current background tile (extended attribute mode)

	uint8_t exram[0x400];
	uint8_t prg_ram[0x10000];
};

bool Mapper5_init(struct Cartridge* c);
uint8_t Mapper5_readPRG_ROM(struct Cartridge* c, uint16_t address);
void Mapper5_writePRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);
uint8_t Mapper5_readPRG_RAM(struct Cartridge* c, uint16_t address);
void Mapper5_writePRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data);
uint8_t Mapper5_readCHR_ROM(struct Cartridge* c, uint16_t address);
void Mapper5_writeCHR(struct Cartridge* c, uint16_t address, uint8_t data);
void Mapper5_notifyPPUAddress(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle);
void Mapper5_notifyPPURegisterWrite(struct Cartridge* c, uint16_t address, uint8_t data);
uint8_t Mapper5_readExpansion(struct Cartridge* c, uint16_t address);
void Mapper5_writeExpansion(struct Cartridge* c, uint16_t address, uint8_t data);
uint8_t Mapper5_readNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram);
void Mapper5_writeNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram, uint8_t data);
void Mapper5_tick(struct Cartridge* c);

#endif