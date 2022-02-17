#ifndef NF_H_PPU
#define NF_H_PPU
#include "NF_Bus.h"
#include <stdint.h>

// Representation of the memory. This maps in the following way:
// 
// $0000-0FFF: Pattern table 0 (on cartridge)
// $1000-1FFF: Pattern table 1 (on cartridge)
// $2000-23FF: Nametable 0
// $2400-27FF: Nametable 1
// $2800-2BFF: Nametable 2
// $2C00-2FFF: Nametable 3
// $3000-3EFF: Mirrors of 2000-2FFF
// $3F00-3F1F: Palette RAM (Note:  $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C )
// $32F0-3FFF: Mirrors of 3F00-3F1F

#define PPU_NAMETABLE_RAM_SIZE 0x0800
#define PPU_PALETTE_RAM_SIZE 0x20
#define PPU_OAM_MEMORY_SIZE 0xFF
#define PPU_SCANLINE_PRERENDER -1
#define PPU_SCANLINE_SCREEN_MAX 240
#define PPU_SCANLINE_MAX 261
#define PPU_CYCLE_MAX 341
#define PPU_CYCLE_SCREEN_MAX 255
#define NAMETABLE_0_ADDRESS 0x2000
#define NAMETABLE_1_ADDRESS 0x2400
#define NAMETABLE_2_ADDRESS 0x2800
#define NAMETABLE_3_ADDRESS 0x2C00
#define PALETTE_RAM_ADDRESS 0x3F00

// A list of all of the registers on the PPU, documented as follows
typedef enum {
	REG_PPUCTRL,
	REG_PPUMASK,
	REG_PPUSTATUS,
	REG_OAMADDR,
	REG_OAMDATA,
	REG_PPUSCROLL,
	REG_PPUADDR,
	REG_PPUDATA,
	REG_OAMDMA
} PPU_REGISTER;

// This is the accepted way to do things, and the one with the best documentation so here we go...
// Thank you, Loopy
union LoopyRegister {
	struct {
		uint16_t coarse_x : 5;
		uint16_t coarse_y : 5;
		uint16_t nametable_x : 1;
		uint16_t nametable_y : 1;
		uint16_t fine_y : 3;
		uint16_t unused_padding : 1; // Want this to align to 16-bits
	};
	uint16_t address;
};

// A structure to represent the PPU
struct PictureProcessingUnit {
	struct NES_Console* bus;

	uint8_t PPU_PaletteMemory[PPU_PALETTE_RAM_SIZE]; // A few wasted bytes, but it will make the code to read and write to this array cleaner
	uint8_t PPU_NametableMemory[PPU_NAMETABLE_RAM_SIZE];
	uint8_t PPU_OAM[PPU_OAM_MEMORY_SIZE];

	// Used by registers that require two writes (PPUSCROLL, PPUADDR) to store state between writes
	uint8_t address_latch; // This one is actually in hardware
	uint8_t delayed_buffer;

	// Used by the beam rendering the screen
	int16_t cycle;
	int16_t scanline;
	// Registers

	// PPUCTRL ($2000)
	// Access: Write-only
	// Bit 0-1: Base nametable address (00 for $2000, 01 for $2400, 10 for $2800, 11 for $2C00)
	// Bit 2: VRAM address increment (0 for 1, 1 for 32)
	// Bit 3: Sprite pattern table for 8x8 sprites (0 for $0000, 1 for $1000). This is ignored in 8x16 mode
	// Bit 4: Background pattern table (0 for $0000, 1 for $1000)
	// Bit 5: Sprite Size mode (0 for 8x8, 1 for 8x16)
	// Bit 6: Unused (This bit was master/slave select, not used by NES)
	// Bit 7: Whether to generate an NMI at the start of VBlank or not (0 for no, 1 for yes)
	uint8_t reg_PPUCTRL;

	// PPUMASK ($2001)
	// Access: Write-only
	// Bit 0: Grayscale (0 for color, 1 for grayscale)
	// Bit 1: Show sprites in leftmost 8 pixels of screen (0 for no, 1 for yes)
	// Bit 2: Show sprites in leftmost 8 pixels of screen (0 for no, 1 for yes)
	// Bit 3: Show background (0 for no, 1 for yes)
	// Bit 4: Show sprites (0 for no, 1 for yes)
	// Bit 5: Emphasize red (0 for no, 1 for yes)
	// Bit 6: Emphasize green (0 for no, 1 for yes)
	// Bit 7: Emphasize blue (0 for no, 1 for yes)
	uint8_t reg_PPUMASK;

	// PPUSTATUS ($2002)
	// Access: Read-only
	// Bit 0-5: The least significant bits previously written into a PPU register.
	// Bit 6: Sprite overflow flag (TO DO: Look into a hardware glitch related to this flag)
	// Bit 7: Sprite 0 hit flag (1 means a hit)
	// Bit 8: Vblank flag (1 means we are in Vblank)
	uint8_t reg_PPUSTATUS;
	
	// OAMADDR ($2003)
	// Access: Write-only
	// This holds an address of where you want to write in the OAM (Object Attribute Memory)
	// However, you can use OAMDMA, which is faster and more efficient
	uint8_t reg_OAMADDR;

	// TO DO: Learn more about this one. It's very glitchy
	uint8_t reg_OAMDATA;

	// PPUSCROLL ($2005)
	// Access: Write-only
	// This uses the address latch. The first write sets the X scroll, then the second write sets the Y scroll
	uint8_t reg_PPUSCROLL;

	// PPUADDR ($2006)
	// Access: Write-only
	// This uses the address latch. The first byte is the hi-byte, the second is the lo-byte.
	// Valid addresses are $0000-$3FFF, with higher addresses being mirrored down.
	uint8_t reg_PPUADDR;

	// PPUDATA ($2007)
	// Access: Read and Write
	// After access, the video memory address will increment by an amount determined by bit 2 of PPUCTRL
	uint8_t reg_PPUDATA;

	// The pair of loop addresses wraps together a lot of how the PPU renders the screen, particularly
	// when it comes to scrolling. See: https://wiki.nesdev.com/w/index.php/PPU_scrolling
	union LoopyRegister vram_addr;
	union LoopyRegister tram_addr;
};

uint8_t NF_PPU_readRegister(struct PictureProcessingUnit* ppu, PPU_REGISTER reg);
void NF_PPU_writeRegister(struct PictureProcessingUnit* ppu, PPU_REGISTER reg, uint8_t data);
struct PictureProcessingUnit* NF_initPPU();
void NF_PPU_tickClock(struct PictureProcessingUnit* ppu);


#endif