#ifndef NES_H_CARTRIDGE
#define NES_H_CARTRIDGE
#include <stdbool.h>
#include <stdint.h>

// Two types of headers are supported by this emulator, iNES and NES 2.0
typedef enum {
	HEADER_INES,
	HEADER_NES_2
} HEADER_TYPE;

typedef enum {
	HORIZONTAL_MAPPING,
	VERTICAL_MAPPING,
	SINGLE_SCREEN_LOWER_MAPPING,	// All four nametables use the first 1KB page (mapper controlled)
	SINGLE_SCREEN_UPPER_MAPPING		// All four nametables use the second 1KB page (mapper controlled)
} SCROLL_MAPPING_TYPE;

// iNES and NES 2.0 Header Format for the first seven bytes
// 0-3:  			Identification String. Must be "NES<EOF>"
// 4:				Number of blocks of 16KB PRG ROM
// 5:				Number of blocks of 8KB of CHR Rom (0 means the board uses CHR RAM)
// 6:	            Bit 0: Hard-wired nametable mirroring type, 0 for Horizontal or Mapper controlled, 1 for Vertical
//					Bit 1: Battery?, 0 for not present, 1 for present
//					Bit 2: Trainer data on cartridge?, 0 for not present, 1 fr present
//					Bit 3: Hard-wired four-screen mode, 0 for No, 1 for Yes	
//					Bit 4-7: Lo-nibble of mapper number
// 7:				Bit 0-1: Console type (0 for NES, 1 for NES vs, 2 for Playchoice 10, 3 for Extended Console Type)			
//					Bit 2-3: Should be "10" if the Header is of type NES 2.0
//					Bit 4-7: Hi-nibble of mapper number
// 
// iNES Format
// 8:              PRG RAM size, in 8kb blocks
// 9:              Bit 0 is 0 if NTSC, and 1 if PAL. All other bits are set to zero. Most emulators ignore this entirely
struct Cartridge {
	uint8_t prg_rom_blocks;		// Blocks of 16kb
	uint8_t chr_rom_blocks;     // Blocks of 8kb, 0 means the board uses CHR RAM
	uint8_t prg_ram_blocks;		// Blocks of 8kb
	HEADER_TYPE header_type;
	bool has_battery;
	bool has_trainer;
	uint8_t trainer[512];
	uint8_t* prg_rom;
	uint8_t* prg_ram;
	uint8_t* chr_rom;
	uint16_t mapper;
	uint8_t flag_6;
	uint8_t flag_7;
	SCROLL_MAPPING_TYPE nametable_mirroring;
	void* mapper_state;			// Mapper-specific registers (bank selects etc.), NULL for mappers without any
	bool irq_asserted;			// The mapper is holding the CPU's /IRQ line low
};



// Load all of the bytes of a file into an array
uint8_t* NES_readROMtoBuffer(const char* filename);

// Take the buffer returned by NES_readROMtoBuffer and turn it into a Cartridge object
struct Cartridge* NES_createCartridgeFromBuffer(char* rom_data);

// Read PRG ROM from a cartridge
uint8_t NES_readCartPRG_ROM(struct Cartridge* c, uint16_t address);

// Write to the PRG ROM address space of a cartridge ($8000-$FFFF). ROM itself can't change, but mappers use these writes to set their registers.
// cpu_cycle is the CPU cycle the write happened on, since some mappers ignore writes on consecutive cycles
void NES_writeCartPRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle);

// Read PRG RAM from a cartridge
uint8_t NES_readCartPRG_RAM(struct Cartridge* c, uint16_t address);

// Write to PRG RAM on a cartridge
void NES_writeCartPRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data);

// Read CHR ROM from a cartridge
uint8_t NES_readCartCHR_ROM(struct Cartridge* c, uint16_t address);

// Write to CHR memory on a cartridge (only has an effect on boards with CHR RAM)
void NES_writeCartCHR(struct Cartridge* c, uint16_t address, uint8_t data);

// Tell the mapper about an address the PPU put on its bus (pattern table and nametable accesses, not palette reads, which stay inside the PPU).
// We pass in the cpu_cycle, since MMC3 (and other(?) mappers?) use the CPU clock to filter the A12 line
void NES_notifyCartPPUAddress(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle);

// Tell the mapper about a CPU write to a PPU register ($2000-$3FFF). MMC5 watches PPUCTRL to know the sprite size
void NES_notifyCartPPURegisterWrite(struct Cartridge* c, uint16_t address, uint8_t data);

// Read from the cartridge expansion area ($4020-$5FFF). Returns open bus if the mapper has nothing there
uint8_t NES_readCartExpansion(struct Cartridge* c, uint16_t address);

// Write to the cartridge expansion area ($4020-$5FFF)
void NES_writeCartExpansion(struct Cartridge* c, uint16_t address, uint8_t data);

// Whether the mapper routes nametable accesses itself, instead of the PPU using the cartridge's nametable_mirroring
bool NES_cartMapsNametables(struct Cartridge* c);

// Read or write a nametable byte through the mapper. offset is 0x000-0xFFF, and ciram is the console's 2KB of nametable RAM
uint8_t NES_readCartNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram);
void NES_writeCartNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram, uint8_t data);

// Tick the mapper once per CPU cycle, for mappers that need to keep time
void NES_tickCart(struct Cartridge* c);

// Free the memory associated with a cartridge
void NES_freeCartridge(struct Cartridge* c);

#endif