#include "NF_PPU.h"
#include "NF_6502.h"
#include "NF_Palette.h"
#include "NF_Cartridge.h"
#include <stdlib.h>
#include <stdio.h>

// Constructor
struct PictureProcessingUnit* NF_initPPU() {
	struct PictureProcessingUnit* newppu = malloc(sizeof(struct PictureProcessingUnit));
	if (newppu == NULL) {
		printf("Error: Could not create PPU object. Out of memory?\n");
		return 0;
	}
	newppu->cycle = 21; // 7 startup cycles for CPU x3 = 21
	newppu->scanline = 0;

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
	addr &= 0x3FFF;  // Mask to the PPU address space (0x0000 - 0x3FFF)

	// Handle nametable memory writes
	if (addr < 0x3F00) {

		// Mirror addresses in range 0x3000 - 0x3EFF down to 0x2000 - 0x2EFF
		if (addr >= 0x3000) {
			addr -= 0x1000;  // Mirror 0x3000-0x3EFF to 0x2000-0x2EFF
		}

		// Nametable mirroring based on cartridge configuration (Horizontal/Vertical)
		if (ppu->bus->ConnectedCartridge->nametable_mirroring == HORIZONTAL_MAPPING) {
			if (addr < 0x0400 || (addr >= 0x0800 && addr < 0x0C00)) {
				ppu->PPU_NametableMemory[addr & 0x03FF] = data;
			}
			else {
				ppu->PPU_NametableMemory[(addr & 0x03FF) + 0x400] = data;
			}
		}
		else if (ppu->bus->ConnectedCartridge->nametable_mirroring == VERTICAL_MAPPING) {
			ppu->PPU_NametableMemory[addr & 0x07FF] = data;  // Vertically map
		}
	}

	// Handle palette RAM writes (0x3F00-0x3FFF, including mirroring)
	else if (addr >= 0x3F00 && addr <= 0x3FFF) {
		addr &= 0x001F;  // Mirror palette RAM (0x3F20-0x3FFF is a mirror of 0x3F00-0x3F1F)

		// Mirror special cases for palette RAM
		if (addr == 0x0010) { addr = 0x0000; }  // 0x3F10 is a mirror of 0x3F00
		if (addr == 0x0014) { addr = 0x0004; }  // 0x3F14 is a mirror of 0x3F04
		if (addr == 0x0018) { addr = 0x0008; }  // 0x3F18 is a mirror of 0x3F08
		if (addr == 0x001C) { addr = 0x000C; }  // 0x3F1C is a mirror of 0x3F0C

		ppu->PPU_PaletteMemory[addr] = data;
	}

	else {
		printf("Error: Tried to write to outside of writable address (PPU) [ %x ]\n", addr);
	}
}


// Read from the PPU address space
uint8_t NF_PPU_readMemory(struct PictureProcessingUnit* ppu, uint16_t addr) {
    addr &= 0x3FFF;  // Mask to the PPU address space (0x0000 - 0x3FFF)

    // Handle cartridge CHR-ROM reads
    if (addr < NAMETABLE_0_ADDRESS) {
        return NF_readCartCHR_ROM(ppu->bus->ConnectedCartridge, addr);
    }

    // Handle nametable memory reads
    else if (addr < 0x3F00) {
        // Mirror addresses in range 0x3000 - 0x3EFF down to 0x2000 - 0x2EFF
        if (addr >= 0x3000) {
            addr -= 0x1000;  // Mirror 0x3000-0x3EFF to 0x2000-0x2EFF
        }

        // Nametable mirroring based on cartridge configuration (Horizontal/Vertical)
        if (ppu->bus->ConnectedCartridge->nametable_mirroring == HORIZONTAL_MAPPING) {
            if (addr < 0x0400 || (addr >= 0x0800 && addr < 0x0C00)) {
                return ppu->PPU_NametableMemory[addr & 0x03FF];
            } else {
                return ppu->PPU_NametableMemory[(addr & 0x03FF) + 0x400];
            }
        } else if (ppu->bus->ConnectedCartridge->nametable_mirroring == VERTICAL_MAPPING) {
            return ppu->PPU_NametableMemory[addr & 0x07FF];  // Vertically map
        }
    }

    // Handle palette RAM reads (0x3F00-0x3FFF, including mirroring)
    else if (addr >= PALETTE_RAM_ADDRESS && addr <= 0x3FFF) {
        addr &= 0x001F;  // Mirror palette RAM (0x3F20-0x3FFF is a mirror of 0x3F00-0x3F1F)

        // Mirror special cases for palette RAM
        if (addr == 0x0010) { addr = 0x0000; }  // 0x3F10 is a mirror of 0x3F00
        if (addr == 0x0014) { addr = 0x0004; }  // 0x3F14 is a mirror of 0x3F04
        if (addr == 0x0018) { addr = 0x0008; }  // 0x3F18 is a mirror of 0x3F08
        if (addr == 0x001C) { addr = 0x000C; }  // 0x3F1C is a mirror of 0x3F0C

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
			// Return the top 3 bits of PPUSTATUS and the lower 5 bits of the delayed buffer
			tmp = (ppu->reg_PPUSTATUS & 0xE0) | (ppu->delayed_buffer & 0x1F);
			ppu->reg_PPUSTATUS &= 0x7F;
			ppu->address_latch = 0x00;
			return tmp;
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
			// Delayed read for VRAM (0x0000-0x3EFF), immediate read for palette data
			if (ppu->vram_addr.address >= 0x3F00) { tmp = NF_PPU_readMemory(ppu, ppu->vram_addr.address); }
			ppu->delayed_buffer = NF_PPU_readMemory(ppu, ppu->vram_addr.address);
			ppu->vram_addr.address += (ppu->reg_PPUCTRL & 0x04) ? 32 : 1; // Horizontal or vertical reading depending on if the bit is set
			return tmp;
		default:
			printf("Error: A non-existant PPU register was attempted to be read.\n");
			return 0x00;
			break;
	}
}

// Write one of the eight PPU registers
void NF_PPU_writeRegister(struct PictureProcessingUnit* ppu, PPU_REGISTER reg, uint8_t data) {
	switch (reg) {
	case REG_PPUCTRL:
		ppu->reg_PPUCTRL = data;
		ppu->tram_addr.nametable_x = (data & 0x01);
		ppu->tram_addr.nametable_y = (data & 0x02) >> 1;
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
		// Handle fine/coarse scrolling
		if (ppu->address_latch == 0) {
			ppu->fine_x = data & 0x07;
			ppu->tram_addr.coarse_x = (data >> 3);
			ppu->address_latch = 1;
		}
		else {
			ppu->tram_addr.coarse_y = (data >> 3);
			ppu->tram_addr.fine_y = (data & 0x07);
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
		ppu->vram_addr.address += (ppu->reg_PPUCTRL & 0x04) ? 32 : 1;
		break;
	default:
		printf("Error: A non-existant PPU register was attempted to be written to.\n");
		break;
	}
}

// Every time the PPU clock ticks, a pixel will be rendered to the screen, and the (virtual) scanline-beam will be adjusted if necessary
// Additionally, a NMI will be emitted if necessary, and the PPU registers will be updated accordingly
void NF_PPU_tickClock(struct PictureProcessingUnit* ppu) {
    ppu->cycle++;

    if (ppu->cycle >= PPU_CYCLE_MAX) {
        ppu->cycle = 0;
        ppu->scanline++;

        // Enter VBlank
        if (ppu->scanline == PPU_SCANLINE_SCREEN_MAX + 1) {
            ppu->reg_PPUSTATUS |= 0x80;
            if (ppu->reg_PPUCTRL & 0x80) { NF_emitNMI(ppu->bus); }
        }

        // Exit VBlank
        if (ppu->scanline > PPU_SCANLINE_MAX) {
            ppu->scanline = 0;
            ppu->reg_PPUSTATUS &= ~0x80;
        }
    }

    if (ppu->scanline < PPU_SCANLINE_SCREEN_MAX) {
        if (ppu->cycle >= 1 && ppu->cycle <= PPU_CYCLE_SCREEN_MAX + 1) {
            // Background render code here
        }
    }
}


