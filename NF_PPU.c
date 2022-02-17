#include "NF_PPU.h"
#include "NF_6502.h"
#include "NF_Palette.h"
#include "NF_Cartridge.h"
#include <stdlib.h>
#include <stdio.h>

struct PictureProcessingUnit* NF_initPPU() {
	struct PictureProcessingUnit* newppu = malloc(sizeof(struct PictureProcessingUnit));
	if (newppu == NULL) {
		printf("Error: Could not create PPU object. Out of memory?\n");
		return 0;
	}
	newppu->cycle = 0;
	newppu->scanline = -1; // TO DO: Research if this should start at 0 or -1

	// Zero out the registers
	newppu->reg_PPUCTRL = 0x00;
	newppu->reg_PPUMASK = 0x00;
	newppu->reg_PPUSTATUS = 0x00;
	newppu->reg_OAMADDR = 0x00;
	newppu->reg_PPUSCROLL = 0x00;
	newppu->reg_PPUADDR = 0x00;
	newppu->reg_PPUDATA = 0x00;
	newppu->delayed_buffer = 0x00;
	newppu->address_latch = 0x00;
	newppu->vram_addr.address = 0x0000;
	newppu->tram_addr.address = 0x0000;
	return newppu;
}

// Write to the PPU address space
void NF_PPU_writeMemory(struct PictureProcessingUnit* ppu, uint16_t addr, uint8_t data) {

	addr &= 0x3FFF;

	// Write to cartridge CHR-ROM
	//if (addr < NAMETABLE_0_ADDRESS) { printf("Error: Cannot write to cartridge CHR-ROM.\n"); }
	// Write to Nametable VRAM
	if (addr >= 0x0000 && addr < 0x3EFF) {
		addr &= 0x0FFF; // 0x3000 - 0x3EFF is a mirror of 0x2000-0x2EFF
		//printf("Written to: %x\n", addr);
		if (ppu->bus->ConnectedCartridge->nametable_mirroring == HORIZONTAL_MAPPING) {
			if (addr >= 0x0000 && addr < 0x0400) { ppu->PPU_NametableMemory[addr] = data; }
			if (addr >= 0x0400 && addr < 0x0800) { ppu->PPU_NametableMemory[addr - 0x400] = data; }
			if (addr >= 0x0800 && addr < 0x0C00) { ppu->PPU_NametableMemory[addr - 0x400] = data; }
			if (addr >= 0x0C00 && addr < 0x1000) { ppu->PPU_NametableMemory[addr - 0x800] = data; }
		}
		if (ppu->bus->ConnectedCartridge->nametable_mirroring == VERTICAL_MAPPING) {
			if (addr >= 0x0000 && addr < 0x0400) { ppu->PPU_NametableMemory[addr] = data; }
			if (addr >= 0x0400 && addr < 0x0800) { ppu->PPU_NametableMemory[addr] = data; }
			if (addr >= 0x0800 && addr < 0x0C00) { ppu->PPU_NametableMemory[addr - 0x800] = data; }
			if (addr >= 0x0C00 && addr < 0x1000) { ppu->PPU_NametableMemory[addr - 0x800] = data; }
		}
	}

	// Write to palette RAM
	else if (addr >= PALETTE_RAM_ADDRESS && addr <= 0x3FFF) {
		addr &= 0x001F;	// 0x3F20 - 0x3FFF is a mirror of 0x3F00 - 0x3F1F
		if (addr == 0x0010) { addr = 0x0000; } // Mirrored
		if (addr == 0x0014) { addr = 0x0004; } // Mirrored
		if (addr == 0x0018) { addr = 0x0008; } // Mirrored
		if (addr == 0x001C) { addr = 0x000C; } // Mirrored
		ppu->PPU_PaletteMemory[addr] = data;
	}

	else { printf("Error: Tried to write to outside of writable address (PPU) [ %x ]\n", addr); }

}

// Read from the PPU address space
uint8_t NF_PPU_readMemory(struct PictureProcessingUnit* ppu, uint16_t addr) {
	
	addr &= 0x3FFF;

	// Read from the cartridge CHR-ROM
	if (addr < NAMETABLE_0_ADDRESS) { return NF_readCartCHR_ROM(ppu->bus->ConnectedCartridge, addr); }

	// Read from Nametable VRAM
	else if (addr >= NAMETABLE_0_ADDRESS && addr < 0x3F00) {
		addr &= 0x0FFF; // 0x3000 - 0x3EFF is a mirror of 0x2000-0x2EFF
		//printf("Read from: %x\n", addr);
		if (ppu->bus->ConnectedCartridge->nametable_mirroring == HORIZONTAL_MAPPING) {
			if (addr >= 0x0000 && addr < 0x0400) { return ppu->PPU_NametableMemory[addr]; }
			if (addr >= 0x0400 && addr < 0x0800) { return ppu->PPU_NametableMemory[addr - 0x400]; }
			if (addr >= 0x0800 && addr < 0x0C00) { return ppu->PPU_NametableMemory[addr - 0x400]; }
			if (addr >= 0x0C00 && addr < 0x1000) { return ppu->PPU_NametableMemory[addr - 0x800]; }
		}
		if (ppu->bus->ConnectedCartridge->nametable_mirroring == VERTICAL_MAPPING) {
			if (addr >= 0x0000 && addr < 0x0400) { return ppu->PPU_NametableMemory[addr]; }
			if (addr >= 0x0400 && addr < 0x0800) { return ppu->PPU_NametableMemory[addr]; }
			if (addr >= 0x0800 && addr < 0x0C00) { return ppu->PPU_NametableMemory[addr - 0x800]; }
			if (addr >= 0x0C00 && addr < 0x1000) { return ppu->PPU_NametableMemory[addr - 0x800]; }
		}
	}

	// Read from palette RAM
	else if (addr >= PALETTE_RAM_ADDRESS && addr <= 0x3FFF) {
		addr &= 0x001F;	// 0x3F20 - 0x3FFF is a mirror of 0x3F00 - 0x3F1F
		if (addr == 0x0010) { addr = 0x0000; } // Mirrored
		if (addr == 0x0014) { addr = 0x0004; } // Mirrored
		if (addr == 0x0018) { addr = 0x0008; } // Mirrored
		if (addr == 0x001C) { addr = 0x000C; } // Mirrored
		return ppu->PPU_PaletteMemory[addr];
	}

	printf("Error: Tried to read from outside of readable address (PPU) [ %x ]\n", addr);

	return 0x00;
}


// Read one of the eight PPU registers
// "Reading any readable port(PPUSTATUS, OAMDATA, or PPUDATA) also fills the latch with the bits read.
// Reading a nominally "write-only" register returns the latch's current value" - wiki.nesdev.com
uint8_t NF_PPU_readRegister(struct PictureProcessingUnit *ppu, PPU_REGISTER reg) {
	uint8_t tmp;
	switch (reg) {
		case REG_PPUCTRL:
			return 0;
			break;
		case REG_PPUMASK:
			return 0;
			break;
		case REG_PPUSTATUS:
			tmp = (ppu->reg_PPUSTATUS & 0b11100000) | ( ppu->delayed_buffer & 0b00011111) ;
			ppu->reg_PPUSTATUS &= 0b01111111;
			ppu->address_latch = 0x00;
			return tmp;
			break;
		case REG_OAMADDR:
			return 0;
			break;
		case REG_OAMDATA:
			return 0;
			break;
		case REG_PPUSCROLL:
			return 0;
			break;
		case REG_PPUADDR:
			return 0;
			break;
		case REG_PPUDATA:
			tmp = ppu->delayed_buffer;
			ppu->delayed_buffer = NF_PPU_readMemory(ppu, ppu->vram_addr.address);
			if (ppu->vram_addr.address >= 0x3F00) { tmp = ppu->delayed_buffer; }
			ppu->vram_addr.address += (((ppu->reg_PPUCTRL & 0b00000100) >> 2) ? 32 : 1);
			return tmp;
			break;
		default:
			printf("Error: A non-existant PPU register was attempted to be read.\n");
			return 0x00;
			break;
	}
}

// Write one of the eight PPU registers
// There is some conflicting information out there. 
void NF_PPU_writeRegister(struct PictureProcessingUnit* ppu, PPU_REGISTER reg, uint8_t data) {
	switch (reg) {
	case REG_PPUCTRL:
		ppu->reg_PPUCTRL = data;
		ppu->tram_addr.nametable_x = (ppu->reg_PPUCTRL & 0b00000001);
		ppu->tram_addr.nametable_y = (ppu->reg_PPUCTRL & 0b00000010) >> 1;
		break;
	case REG_PPUMASK:
		ppu->reg_PPUMASK = data;
		break;
	case REG_PPUSTATUS:
		printf("Error: PPUSTATUS is a read-only PPU register.\n");
		break;
	case REG_OAMADDR:
		// To Do: Flesh this out
		break;
	case REG_OAMDATA:
		// To Do: Flesh this out
		break;
	case REG_PPUSCROLL:
		if (ppu->address_latch == 0) {
			ppu->tram_addr.coarse_x = (data >> 3);
			ppu->address_latch = 1;
		}
		else {
			ppu->tram_addr.coarse_y = (data >> 3);
			ppu->address_latch = 0;
		}
		break;
	case REG_PPUADDR:
		if (ppu->address_latch == 0) {
			ppu->tram_addr.address = (uint16_t)((data & 0x3F) << 8) | (ppu->tram_addr.address & 0x00FF);
			ppu->address_latch = 1;
		}
		else {
			ppu->tram_addr.address = (ppu->tram_addr.address & 0xFF00) | data;
			ppu->vram_addr.address = ppu->tram_addr.address;
			ppu->address_latch = 0;
		}
		break;
	case REG_PPUDATA:
		NF_PPU_writeMemory(ppu, ppu->vram_addr.address, data);
		ppu->vram_addr.address += (((ppu->reg_PPUCTRL & 0b00000100) >> 2) ? 32 : 1); // How much the address increases by depends on if we are in 8x8 sprite mode, or 8x16 sprite mode
		break;
	default:
		printf("Error: A non-existant PPU register was attempted to be written to.\n");
		break;
	}
}


uint8_t* rgb;
struct NF_Pixel pxl;
uint8_t tx;
uint8_t ty;
// Every time the PPU clock ticks, a pixel will be rendered to the screen, and the (virtual) scanline-beam will be adjusted if necessary
// Additionally, a NMI will be emitted if necessary, and the PPU registers will be updated accordingly
void NF_PPU_tickClock(struct PictureProcessingUnit* ppu) {

// TO DO: Implement Rendering

}