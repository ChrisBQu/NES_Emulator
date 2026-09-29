#include "NF_PPU.h"
#include "NF_6502.h"
#include "NF_Palette.h"
#include "NF_Cartridge.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Constructor
struct PictureProcessingUnit* NF_initPPU() {
	struct PictureProcessingUnit* newppu = malloc(sizeof(struct PictureProcessingUnit));
	if (newppu == NULL) {
		printf("Error: Could not create PPU object. Out of memory?\n");
		return 0;
	}
	newppu->cycle = 21; // 7 startup cycles for CPU x3 = 21
	newppu->scanline = 0;
	newppu->odd_frame = false;
	newppu->frame_complete = false;
	newppu->fine_x = 0x00;
	newppu->reg_OAMDATA = 0x00;

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
	memset(newppu->PPU_PaletteMemory,   0, sizeof(newppu->PPU_PaletteMemory));
	memset(newppu->PPU_NametableMemory, 0, sizeof(newppu->PPU_NametableMemory));
	memset(newppu->PPU_OAM,             0, sizeof(newppu->PPU_OAM));
	newppu->bg_next_tile_id     = 0x00;
	newppu->bg_next_tile_attrib = 0x00;
	newppu->bg_next_tile_lsb    = 0x00;
	newppu->bg_next_tile_msb    = 0x00;
	newppu->bg_shifter_pattern_lo = 0x0000;
	newppu->bg_shifter_pattern_hi = 0x0000;
	newppu->bg_shifter_attrib_lo  = 0x0000;
	newppu->bg_shifter_attrib_hi  = 0x0000;
	memset(newppu->sprite_scanline, 0xFF, sizeof(newppu->sprite_scanline));
	memset(newppu->sprite_shifter_pattern_lo, 0, sizeof(newppu->sprite_shifter_pattern_lo));
	memset(newppu->sprite_shifter_pattern_hi, 0, sizeof(newppu->sprite_shifter_pattern_hi));
	newppu->sprite_count = 0;
	newppu->sprite_zero_on_line = false;
	newppu->sprite_zero_being_drawn = false;
	return newppu;
}

// Copy 256 bytes into OAM starting at OAMADDR (wrapping around), as the OAM DMA at $4014 does
void NF_PPU_writeOAMDMA(struct PictureProcessingUnit* ppu, const uint8_t* page) {
	for (int i = 0; i < PPU_OAM_MEMORY_SIZE; i++) {
		ppu->PPU_OAM[(uint8_t)(ppu->reg_OAMADDR + i)] = page[i];
	}
}

// Helper function
// The cartridge can see every address the PPU puts on its external bus, and some mappers (MMC3) watch it to count scanlines.
static void PPU_driveBus(struct PictureProcessingUnit* ppu, uint16_t addr) {
	if (addr >= 0x3F00) { return; }
	NF_notifyCartPPUAddress(ppu->bus->ConnectedCartridge, addr, ppu->bus->ConnectedProcessor->cycle_count);
}

// Helper function
// Rendering is active on the visible and pre-render scanlines while the background or sprites are enabled
static bool PPU_isRendering(struct PictureProcessingUnit* ppu) {
	return (ppu->reg_PPUMASK & 0x18) && (ppu->scanline < 240 || ppu->scanline == 261);
}

// Write to the PPU address space
void NF_PPU_writeMemory(struct PictureProcessingUnit* ppu, uint16_t addr, uint8_t data) {
	addr &= 0x3FFF;  // Mask to the PPU address space (0x0000 - 0x3FFF)
	PPU_driveBus(ppu, addr);

	// Handle pattern table writes (only possible on boards that use CHR RAM instead of CHR ROM)
	if (addr < NAMETABLE_0_ADDRESS) {
		NF_writeCartCHR(ppu->bus->ConnectedCartridge, addr, data);
	}

	// Handle nametable memory writes
	else if (addr < 0x3F00) {

		// Mirror addresses in range 0x3000 - 0x3EFF down to 0x2000 - 0x2EFF
		if (addr >= 0x3000) {
			addr -= 0x1000;
		}

		// Convert from PPU address space to a nametable-relative offset (0x000-0xFFF)
		addr -= 0x2000;

		// Some mappers (MMC5) re-route where nametables come from
		if (NF_cartMapsNametables(ppu->bus->ConnectedCartridge)) {
			NF_writeCartNametable(ppu->bus->ConnectedCartridge, addr, ppu->PPU_NametableMemory, data);
			return;
		}

		// Nametable mirroring based on cartridge configuration (Horizontal/Vertical)
		// We have 2KB of physical nametable RAM split into two 1KB pages:
		//   Page 0: PPU_NametableMemory[0x000-0x3FF]
		//   Page 1: PPU_NametableMemory[0x400-0x7FF]
		if (ppu->bus->ConnectedCartridge->nametable_mirroring == HORIZONTAL_MAPPING) {
			// Horizontal: NT0 and NT1 share page 0; NT2 and NT3 share page 1
			if (addr < 0x0800) {
				ppu->PPU_NametableMemory[addr & 0x03FF] = data;
			}
			else {
				ppu->PPU_NametableMemory[(addr & 0x03FF) + 0x400] = data;
			}
		}
		else if (ppu->bus->ConnectedCartridge->nametable_mirroring == VERTICAL_MAPPING) {
			// Vertical: NT0 and NT2 share page 0; NT1 and NT3 share page 1
			ppu->PPU_NametableMemory[addr & 0x07FF] = data;
		}
		else if (ppu->bus->ConnectedCartridge->nametable_mirroring == SINGLE_SCREEN_LOWER_MAPPING) {
			// Single screen: all four nametables share page 0
			ppu->PPU_NametableMemory[addr & 0x03FF] = data;
		}
		else if (ppu->bus->ConnectedCartridge->nametable_mirroring == SINGLE_SCREEN_UPPER_MAPPING) {
			// Single screen: all four nametables share page 1
			ppu->PPU_NametableMemory[(addr & 0x03FF) + 0x400] = data;
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
    PPU_driveBus(ppu, addr);

    // Handle cartridge CHR-ROM reads
    if (addr < NAMETABLE_0_ADDRESS) {
        return NF_readCartCHR_ROM(ppu->bus->ConnectedCartridge, addr);
    }

    // Handle nametable memory reads
    else if (addr < 0x3F00) {
        // Mirror addresses in range 0x3000 - 0x3EFF down to 0x2000 - 0x2EFF
        if (addr >= 0x3000) {
            addr -= 0x1000;
        }

        // Convert from PPU address space to a nametable-relative offset (0x000-0xFFF)
        addr -= 0x2000;

        // Some mappers (MMC5) decide where each nametable comes from themselves
        if (NF_cartMapsNametables(ppu->bus->ConnectedCartridge)) {
            return NF_readCartNametable(ppu->bus->ConnectedCartridge, addr, ppu->PPU_NametableMemory);
        }

        // Nametable mirroring based on cartridge configuration (Horizontal/Vertical)
        if (ppu->bus->ConnectedCartridge->nametable_mirroring == HORIZONTAL_MAPPING) {
            // Horizontal: NT0 and NT1 share page 0; NT2 and NT3 share page 1
            if (addr < 0x0800) {
                return ppu->PPU_NametableMemory[addr & 0x03FF];
            } else {
                return ppu->PPU_NametableMemory[(addr & 0x03FF) + 0x400];
            }
        } else if (ppu->bus->ConnectedCartridge->nametable_mirroring == VERTICAL_MAPPING) {
            // Vertical: NT0 and NT2 share page 0; NT1 and NT3 share page 1
            return ppu->PPU_NametableMemory[addr & 0x07FF];
        } else if (ppu->bus->ConnectedCartridge->nametable_mirroring == SINGLE_SCREEN_LOWER_MAPPING) {
            // Single screen: all four nametables share page 0
            return ppu->PPU_NametableMemory[addr & 0x03FF];
        } else if (ppu->bus->ConnectedCartridge->nametable_mirroring == SINGLE_SCREEN_UPPER_MAPPING) {
            // Single screen: all four nametables share page 1
            return ppu->PPU_NametableMemory[(addr & 0x03FF) + 0x400];
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
			return ppu->PPU_OAM[ppu->reg_OAMADDR];
		case REG_PPUSCROLL:
			return 0;
			break;
		case REG_PPUADDR:
			return 0;
			break;
		case REG_PPUDATA:
			tmp = ppu->delayed_buffer;
			// Delayed read for VRAM (0x0000-0x3EFF), immediate read for palette data
			if ((ppu->vram_addr.address & 0x3FFF) >= 0x3F00) { tmp = NF_PPU_readMemory(ppu, ppu->vram_addr.address); }
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
		// Enabling NMI while already in VBlank immediately triggers an NMI
		if (!(ppu->reg_PPUCTRL & 0x80) && (data & 0x80) && (ppu->reg_PPUSTATUS & 0x80)) {
			NF_emitNMI(ppu->bus);
		}
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
		ppu->reg_OAMADDR = data;
		break;
	case REG_OAMDATA:
		// Each write increments OAMADDR (wrapping at 0xFF)
		ppu->PPU_OAM[ppu->reg_OAMADDR++] = data;
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

			// Outside of rendering, the VRAM address sits on the bus. Some games toggle A12 this way to tick the MMC3 counter
			if (!PPU_isRendering(ppu)) { PPU_driveBus(ppu, ppu->vram_addr.address & 0x3FFF); }
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

// --- Background rendering helpers ---

// Increment the horizontal scroll component of vram_addr, flipping the horizontal nametable bit when coarse_x wraps
static void PPU_incrementScrollX(struct PictureProcessingUnit* ppu) {
    if (!(ppu->reg_PPUMASK & 0x08) && !(ppu->reg_PPUMASK & 0x10)) { return; }
    if (ppu->vram_addr.coarse_x == 31) {
        ppu->vram_addr.coarse_x = 0;
        ppu->vram_addr.nametable_x = ~ppu->vram_addr.nametable_x & 0x01;
    } else {
        ppu->vram_addr.coarse_x++;
    }
}

// Increment the vertical scroll component, handling fine_y overflow into coarse_y and nametable_y
static void PPU_incrementScrollY(struct PictureProcessingUnit* ppu) {
    if (!(ppu->reg_PPUMASK & 0x08) && !(ppu->reg_PPUMASK & 0x10)) { return; }
    if (ppu->vram_addr.fine_y < 7) {
        ppu->vram_addr.fine_y++;
    } else {
        ppu->vram_addr.fine_y = 0;
        if (ppu->vram_addr.coarse_y == 29) {
            // Row 29 is the last row of tiles; wrap and flip the vertical nametable
            ppu->vram_addr.coarse_y = 0;
            ppu->vram_addr.nametable_y = ~ppu->vram_addr.nametable_y & 0x01;
        } else if (ppu->vram_addr.coarse_y == 31) {
            // coarse_y can be forced past 29 by the CPU; just wrap without flipping
            ppu->vram_addr.coarse_y = 0;
        } else {
            ppu->vram_addr.coarse_y++;
        }
    }
}

// Copy horizontal position bits from tram_addr into vram_addr (done at cycle 257 each scanline)
static void PPU_transferAddressX(struct PictureProcessingUnit* ppu) {
    if (!(ppu->reg_PPUMASK & 0x08) && !(ppu->reg_PPUMASK & 0x10)) { return; }
    ppu->vram_addr.nametable_x = ppu->tram_addr.nametable_x;
    ppu->vram_addr.coarse_x    = ppu->tram_addr.coarse_x;
}

// Copy vertical position bits from tram_addr into vram_addr (done during pre-render scanline)
static void PPU_transferAddressY(struct PictureProcessingUnit* ppu) {
    if (!(ppu->reg_PPUMASK & 0x08) && !(ppu->reg_PPUMASK & 0x10)) { return; }
    ppu->vram_addr.fine_y      = ppu->tram_addr.fine_y;
    ppu->vram_addr.nametable_y = ppu->tram_addr.nametable_y;
    ppu->vram_addr.coarse_y    = ppu->tram_addr.coarse_y;
}

// Load the latched tile bytes into the low bytes of the 16-bit shift registers
static void PPU_loadBackgroundShifters(struct PictureProcessingUnit* ppu) {
    ppu->bg_shifter_pattern_lo = (ppu->bg_shifter_pattern_lo & 0xFF00) | ppu->bg_next_tile_lsb;
    ppu->bg_shifter_pattern_hi = (ppu->bg_shifter_pattern_hi & 0xFF00) | ppu->bg_next_tile_msb;
    // Expand the 2-bit palette index into a full byte (0x00 or 0xFF) so it shifts cleanly
    ppu->bg_shifter_attrib_lo  = (ppu->bg_shifter_attrib_lo  & 0xFF00) | ((ppu->bg_next_tile_attrib & 0x01) ? 0xFF : 0x00);
    ppu->bg_shifter_attrib_hi  = (ppu->bg_shifter_attrib_hi  & 0xFF00) | ((ppu->bg_next_tile_attrib & 0x02) ? 0xFF : 0x00);
}

// Shift all four shift registers left by one bit each cycle
static void PPU_updateShifters(struct PictureProcessingUnit* ppu) {
    if (!(ppu->reg_PPUMASK & 0x08)) { return; }
    ppu->bg_shifter_pattern_lo <<= 1;
    ppu->bg_shifter_pattern_hi <<= 1;
    ppu->bg_shifter_attrib_lo  <<= 1;
    ppu->bg_shifter_attrib_hi  <<= 1;
}


// Helper for sprite rendering
// Reverse the bits of a byte (used for horizontally flipped sprites)
static uint8_t PPU_reverseBits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// Helper for sprite rendering
// Empty the list of sprites selected for the next scanline
static void PPU_clearSpriteScanline(struct PictureProcessingUnit* ppu) {
    memset(ppu->sprite_scanline, 0xFF, sizeof(ppu->sprite_scanline));
    memset(ppu->sprite_shifter_pattern_lo, 0, sizeof(ppu->sprite_shifter_pattern_lo));
    memset(ppu->sprite_shifter_pattern_hi, 0, sizeof(ppu->sprite_shifter_pattern_hi));
    ppu->sprite_count = 0;
    ppu->sprite_zero_on_line = false;
}

// Helper for sprite rendering
// Select the (up to 8) sprites in OAM that are visible on the next scanline.
// OAM stores each sprite's Y position minus 1, so comparing against the current scanline selects sprites for the next one
static void PPU_evaluateSprites(struct PictureProcessingUnit* ppu) {
    PPU_clearSpriteScanline(ppu);
    uint8_t height = (ppu->reg_PPUCTRL & 0x20) ? 16 : 8;

    for (int n = 0; n < 64; n++) {
        const uint8_t* entry = &ppu->PPU_OAM[n * 4];
        int diff = ppu->scanline - entry[0];
        if (diff < 0 || diff >= height) { continue; }

        if (ppu->sprite_count < PPU_MAX_SPRITES_PER_SCANLINE) {
            struct NF_SpriteEntry* s = &ppu->sprite_scanline[ppu->sprite_count++];
            s->y = entry[0];
            s->id = entry[1];
            s->attr = entry[2];
            s->x = entry[3];
            if (n == 0) { ppu->sprite_zero_on_line = true; }
        }
        else {
            // More than 8 sprites on this line (this ignores the hardware bug in the real overflow logic)
            ppu->reg_PPUSTATUS |= 0x20;
            break;
        }
    }
}

// Helper for sprite rendering
// Find the pattern table address of one row of a sprite's tile
static uint16_t PPU_spritePatternAddress(struct PictureProcessingUnit* ppu, uint8_t id, uint8_t attr, uint8_t row) {
    bool flip_vertical = (attr & 0x80) != 0;

    // 8x8 sprites: PPUCTRL bit 3 selects the pattern table
    if (!(ppu->reg_PPUCTRL & 0x20)) {
        if (flip_vertical) { row = 7 - row; }
        return ((ppu->reg_PPUCTRL & 0x08) ? 0x1000 : 0x0000) | ((uint16_t)id << 4) | row;
    }

    // 8x16 sprites: bit 0 of the tile index selects the pattern table, and the sprite uses tiles (id & 0xFE) and (id & 0xFE) + 1
    if (flip_vertical) { row = 15 - row; }
    uint8_t tile = (id & 0xFE) + (row >= 8 ? 1 : 0);
    return ((id & 0x01) ? 0x1000 : 0x0000) | ((uint16_t)tile << 4) | (row & 0x07);
}

// Helper for sprite rendering
// Cycles 257-320 fetch the patterns for the next scanline's sprites, taking 8 cycles for each of the 8 slots. Like background tiles, each
// slot makes two (garbage) nametable fetches and then fetches the low and high pattern bytes. Empty slots still fetch tile $FF and throw
// the result away. Mappers that count scanlines with PPU A12 rely on these fetches happening even when there are no sprites
static void PPU_fetchSpriteSlot(struct PictureProcessingUnit* ppu) {
    int slot = (ppu->cycle - 257) / 8;
    bool used = slot < ppu->sprite_count;
    struct NF_SpriteEntry* s = &ppu->sprite_scanline[slot];
    uint16_t addr = used ? PPU_spritePatternAddress(ppu, s->id, s->attr, (uint8_t)(ppu->scanline - s->y))
                         : PPU_spritePatternAddress(ppu, 0xFF, 0x00, 0);

    switch ((ppu->cycle - 257) % 8) {
        case 0:
        case 2:
            NF_PPU_readMemory(ppu, 0x2000 | (ppu->vram_addr.address & 0x0FFF));
            break;
        case 4: {
            // Horizontal flip: reverse the row so the leftmost pixel is always in bit 7
            uint8_t lo = NF_PPU_readMemory(ppu, addr);
            if (used) { ppu->sprite_shifter_pattern_lo[slot] = (s->attr & 0x40) ? PPU_reverseBits(lo) : lo; }
            break;
        }
        case 6: {
            uint8_t hi = NF_PPU_readMemory(ppu, addr + 8);
            if (used) { ppu->sprite_shifter_pattern_hi[slot] = (s->attr & 0x40) ? PPU_reverseBits(hi) : hi; }
            break;
        }
    }
}

// Helper for sprite rendering
// Count each sprite's X position down to 0, then shift its pattern out one pixel per cycle
static void PPU_updateSpriteShifters(struct PictureProcessingUnit* ppu) {
    if (!(ppu->reg_PPUMASK & 0x10)) { return; }
    for (int i = 0; i < ppu->sprite_count; i++) {
        if (ppu->sprite_scanline[i].x > 0) {
            ppu->sprite_scanline[i].x--;
        } else {
            ppu->sprite_shifter_pattern_lo[i] <<= 1;
            ppu->sprite_shifter_pattern_hi[i] <<= 1;
        }
    }
}

// Every time the PPU clock ticks, a pixel will be rendered to the screen, and the (virtual) scanline-beam will be adjusted if necessary
// Additionally, a NMI will be emitted if necessary, and the PPU registers will be updated accordingly
void NF_PPU_tickClock(struct PictureProcessingUnit* ppu) {
    ppu->cycle++;

    if (ppu->cycle >= PPU_CYCLE_MAX) {
        ppu->cycle = 0;
        ppu->scanline++;

        // End of frame: scanline 261 was the pre-render line, reset for next frame
        if (ppu->scanline > PPU_SCANLINE_MAX) {
            ppu->scanline = 0;
            ppu->odd_frame = !ppu->odd_frame;
        }
    }

    // On odd frames with rendering enabled, the PPU skips one cycle (the idle cycle 0 of scanline 0)
    if (ppu->scanline == 0 && ppu->cycle == 0 && ppu->odd_frame && (ppu->reg_PPUMASK & 0x18)) {
        ppu->cycle = 1;
    }

    // Pre-render scanline (261) and visible scanlines (0-239) share most rendering logic
    if (ppu->scanline == 261 || (ppu->scanline >= 0 && ppu->scanline < 240)) {

        // The PPU makes no memory fetches while rendering is disabled, so nothing appears on the bus for mappers to see
        bool rendering = (ppu->reg_PPUMASK & 0x18) != 0;

        // Clear status flags at the very start of the pre-render scanline
        if (ppu->scanline == 261 && ppu->cycle == 1) {
            ppu->reg_PPUSTATUS &= ~0x80;  // VBlank
            ppu->reg_PPUSTATUS &= ~0x40;  // Sprite 0 hit
            ppu->reg_PPUSTATUS &= ~0x20;  // Sprite overflow
        }

        // The nametable fetch for the third tile. It already happened at cycle 340 (so v hasn't moved since), but real hardware
        // fetches it here, and MMC5 detects the start of a scanline by seeing this address read three times in a row
        if (rendering && ppu->cycle == 1) {
            ppu->bg_next_tile_id = NF_PPU_readMemory(ppu, 0x2000 | (ppu->vram_addr.address & 0x0FFF));
        }

        // Background tile fetch pipeline: cycles 2-256 (visible) and 321-337 (prefetch next scanline's first two tiles).
        // Cycle 257 is left to the sprite fetches, which start with a nametable fetch of their own
        if (rendering && ((ppu->cycle >= 2 && ppu->cycle < 257) || (ppu->cycle >= 321 && ppu->cycle < 338))) {
            PPU_updateShifters(ppu);

            // Each group of 8 cycles fetches one tile's worth of data in four steps
            switch ((ppu->cycle - 1) % 8) {
                case 0:
                    // Step 1: Load previous latch data into shift registers, then fetch the nametable byte for the next tile
                    PPU_loadBackgroundShifters(ppu);
                    ppu->bg_next_tile_id = NF_PPU_readMemory(ppu,
                        0x2000 | (ppu->vram_addr.address & 0x0FFF));
                    break;
                case 2:
                    // Step 2: Fetch the attribute byte that covers this tile's 4x4-tile block
                    ppu->bg_next_tile_attrib = NF_PPU_readMemory(ppu,
                        0x23C0
                        | ((uint16_t)ppu->vram_addr.nametable_y << 11)
                        | ((uint16_t)ppu->vram_addr.nametable_x << 10)
                        | ((ppu->vram_addr.coarse_y >> 2) << 3)
                        | (ppu->vram_addr.coarse_x >> 2));
                    // Each attribute byte covers a 4x4 tile area; select the correct 2-bit palette for this 2x2 quadrant
                    if (ppu->vram_addr.coarse_y & 0x02) { ppu->bg_next_tile_attrib >>= 4; }
                    if (ppu->vram_addr.coarse_x & 0x02) { ppu->bg_next_tile_attrib >>= 2; }
                    ppu->bg_next_tile_attrib &= 0x03;
                    break;
                case 4:
                    // Step 3: Fetch the low bitplane byte for this tile row from the pattern table
                    ppu->bg_next_tile_lsb = NF_PPU_readMemory(ppu,
                        ((ppu->reg_PPUCTRL & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)ppu->bg_next_tile_id << 4)
                        + ppu->vram_addr.fine_y);
                    break;
                case 6:
                    // Step 4: Fetch the high bitplane byte (8 bytes after the low byte in the pattern table)
                    ppu->bg_next_tile_msb = NF_PPU_readMemory(ppu,
                        ((ppu->reg_PPUCTRL & 0x10) ? 0x1000 : 0x0000)
                        + ((uint16_t)ppu->bg_next_tile_id << 4)
                        + ppu->vram_addr.fine_y + 8);
                    break;
                case 7:
                    // Step 5: Done with this tile; advance to the next column
                    PPU_incrementScrollX(ppu);
                    break;
            }
        }

        // End of visible pixels: advance to the next row
        if (ppu->cycle == 256) {
            PPU_incrementScrollY(ppu);
        }

        // Sprites only shift during the visible part of the scanline (the X counters are reloaded by the next evaluation)
        if (ppu->scanline < 240 && ppu->cycle >= 2 && ppu->cycle < 258) {
            PPU_updateSpriteShifters(ppu);
        }

        // Restore horizontal position from tram at the start of hblank
        if (ppu->cycle == 257) {
            PPU_loadBackgroundShifters(ppu);
            PPU_transferAddressX(ppu);

            // Select the sprites for the next scanline. Sprites never appear on scanline 0, so the pre-render line just clears the list
            if (ppu->scanline < 240) { PPU_evaluateSprites(ppu); }
            else { PPU_clearSpriteScanline(ppu); }
        }

        // Fetch the pattern data for the sprites selected for the next scanline
        if (rendering && ppu->cycle >= 257 && ppu->cycle <= 320) {
            PPU_fetchSpriteSlot(ppu);
        }

        // Dummy nametable fetches at end of scanline (cycles 338 and 340)
        if (rendering && (ppu->cycle == 338 || ppu->cycle == 340)) {
            ppu->bg_next_tile_id = NF_PPU_readMemory(ppu,
                0x2000 | (ppu->vram_addr.address & 0x0FFF));
        }

        // Pre-render scanline only: restore vertical position from tram (cycles 280-304)
        if (ppu->scanline == 261 && ppu->cycle >= 280 && ppu->cycle < 305) {
            PPU_transferAddressY(ppu);
        }
    }

    // Enter VBlank at scanline 241, cycle 1 (the flag is cleared again at scanline 261, cycle 1)
    if (ppu->scanline == 241 && ppu->cycle == 1) {
        ppu->reg_PPUSTATUS |= 0x80;
        if (ppu->reg_PPUCTRL & 0x80) { NF_emitNMI(ppu->bus); }
    }

    // Output one visible pixel
    if (ppu->scanline >= 0 && ppu->scanline < 240 && ppu->cycle >= 1 && ppu->cycle <= 256) {
        uint8_t bg_pixel   = 0x00;
        uint8_t bg_palette = 0x00;

        // PPUMASK bit 1 hides the background in the leftmost 8 pixels of the screen
        if ((ppu->reg_PPUMASK & 0x08) && ((ppu->reg_PPUMASK & 0x02) || ppu->cycle > 8)) {
            // fine_x selects which bit pair in the shift registers represents the current pixel
            uint16_t bit_mux = 0x8000 >> ppu->fine_x;
            uint8_t p0 = (ppu->bg_shifter_pattern_lo & bit_mux) ? 1 : 0;
            uint8_t p1 = (ppu->bg_shifter_pattern_hi & bit_mux) ? 1 : 0;
            bg_pixel = (p1 << 1) | p0;

            uint8_t pal0 = (ppu->bg_shifter_attrib_lo & bit_mux) ? 1 : 0;
            uint8_t pal1 = (ppu->bg_shifter_attrib_hi & bit_mux) ? 1 : 0;
            bg_palette = (pal1 << 1) | pal0;
        }

        // Find the sprite pixel: the first sprite (in OAM order) with a non-transparent pixel here wins,
        // even if it is behind the background and a later sprite is in front of it
        uint8_t fg_pixel    = 0x00;
        uint8_t fg_palette  = 0x00;
        bool    fg_in_front = false;
        ppu->sprite_zero_being_drawn = false;

        // PPUMASK bit 2 hides sprites in the leftmost 8 pixels of the screen
        if ((ppu->reg_PPUMASK & 0x10) && ((ppu->reg_PPUMASK & 0x04) || ppu->cycle > 8)) {
            for (int i = 0; i < ppu->sprite_count; i++) {
                if (ppu->sprite_scanline[i].x != 0) { continue; }

                uint8_t p0 = (ppu->sprite_shifter_pattern_lo[i] & 0x80) ? 1 : 0;
                uint8_t p1 = (ppu->sprite_shifter_pattern_hi[i] & 0x80) ? 1 : 0;
                fg_pixel = (p1 << 1) | p0;

                if (fg_pixel != 0) {
                    fg_palette  = (ppu->sprite_scanline[i].attr & 0x03) + 4;  // Sprite palettes are 4-7 ($3F10-$3F1F)
                    fg_in_front = !(ppu->sprite_scanline[i].attr & 0x20);
                    if (i == 0 && ppu->sprite_zero_on_line) { ppu->sprite_zero_being_drawn = true; }
                    break;
                }
            }
        }

        // Combine the background and sprite pixels
        uint8_t pixel_value   = 0x00;
        uint8_t pixel_palette = 0x00;
        if (bg_pixel == 0 && fg_pixel == 0) {
            // Both transparent: the universal backdrop colour at $3F00, regardless of which palette was selected
            pixel_value = 0x00;
            pixel_palette = 0x00;
        }
        else if (bg_pixel == 0) {
            pixel_value = fg_pixel;
            pixel_palette = fg_palette;
        }
        else if (fg_pixel == 0) {
            pixel_value = bg_pixel;
            pixel_palette = bg_palette;
        }
        else {
            // Both opaque: the sprite's priority bit decides, and this is where sprite 0 hit is detected
            if (fg_in_front) {
                pixel_value = fg_pixel;
                pixel_palette = fg_palette;
            } else {
                pixel_value = bg_pixel;
                pixel_palette = bg_palette;
            }

            // Sprite 0 hit never happens at x = 255. The left-8-pixel masks are already handled, as masked pixels are transparent
            if (ppu->sprite_zero_being_drawn && ppu->cycle != 256) {
                ppu->reg_PPUSTATUS |= 0x40;
            }
        }

        // Look up the NES system colour for this palette+pixel combination.
        // Mask to 6 bits: the NES hardware ignores bits 6-7 of palette entries.
        uint8_t palette_index = NF_PPU_readMemory(ppu, 0x3F00 + (pixel_palette << 2) + pixel_value) & 0x3F;
        if (ppu->reg_PPUMASK & 0x01) { palette_index &= 0x30; }  // Grayscale mode
        const uint8_t* color  = NF_getNESColor(palette_index);

        struct NF_Pixel pixel;
        pixel.x = ppu->cycle - 1;   // cycles are 1-indexed; x is 0-indexed
        pixel.y = ppu->scanline;
        pixel.r = color[0];
        pixel.g = color[1];
        pixel.b = color[2];

        if (ppu->bus->imageOutFunc != NULL) {
            ppu->bus->imageOutFunc(pixel);
        }

        // Last visible pixel of the frame has been output
        if (ppu->scanline == 239 && ppu->cycle == 256) {
            ppu->frame_complete = true;
        }
    }
}


