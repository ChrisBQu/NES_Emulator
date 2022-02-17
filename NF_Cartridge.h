#ifndef NF_H_CARTRIDGE
#define NF_H_CARTRIDGE
#include <stdbool.h>
#include <stdint.h>

// Two types of headers are supported by this emulator, iNES and NES 2.0
typedef enum {
	HEADER_INES,
	HEADER_NES_2
} HEADER_TYPE;

typedef enum {
	HORIZONTAL_MAPPING,
	VERTICAL_MAPPING
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
	uint8_t* chr_rom;
	uint16_t mapper;
	uint8_t flag_6;
	uint8_t flag_7;
	SCROLL_MAPPING_TYPE nametable_mirroring;
};



// Load all of the bytes of a file into an array
uint8_t * NF_readROMtoBuffer(const char* filename);

// Take the buffer returned by NF_reqadROMtoBuffer and turn it into a Cartridge object
struct Cartridge * NF_createCartridgeFromBuffer(char* rom_data);

// Read PRG ROM from a cartridge
uint8_t NF_readCartPRG_ROM(struct Cartridge* c, uint16_t address);

// Read CHR ROM from a cartridge
uint8_t NF_readCartCHR_ROM(struct Cartridge* c, uint16_t address);

#endif