#include "NF_Cartridge.h"
#include <stdio.h>
#include <malloc.h>

#define PRG_ROM_BLOCK_SIZE 16384
#define CHR_ROM_BLOCK_SIZE 8192
#define TRAINER_BLOCK_SIZE 512

struct Cartridge * NF_createCartridgeFromBuffer(char* rom_data) {

	struct Cartridge *Cart = malloc(sizeof(struct Cartridge));

	if (Cart == NULL) {
		printf("Error: Could not create cartridge object. Out of memory?\n");
		return NULL;
	}

	// On a valid NES rom with a header, the first four bytes will be [0x43, 0x45, 0x53, 0x1A], which spell out "NES<EOF>"
	if (rom_data[0] != 0x4E || rom_data[1] != 0x45 || rom_data[2] != 0x53 || rom_data[3] != 0x1A) {
		printf("Error: The ROM has an invalid header, is not a valid NES rom, or is corrupted.\n");
		free(Cart);
		return NULL;
	}

	Cart->prg_rom_blocks = rom_data[4];
	Cart->chr_rom_blocks = rom_data[5];
	Cart->flag_6 = rom_data[6];
	Cart->nametable_mirroring = (SCROLL_MAPPING_TYPE)(rom_data[6] & 0b01000000);
	Cart->flag_7 = rom_data[7];
	Cart->has_battery = ((rom_data[6] & 0b00000010) != 0);
	Cart->has_trainer = ((rom_data[6] & 0b00000100) != 0);
	Cart->mapper = ((rom_data[6] & 0b00001111) & (rom_data[7] & 0b11110000));

	// Check if the header is NES 2.0
	if ((rom_data[7] & 0x0C) == 0x08) {
		Cart->header_type = HEADER_NES_2;
		// TO DO: Implement NES 2.0 Header support
	}

	// Check if the header is iNES, or is invalid
	else if ((rom_data[7] & 0x0C ) == 0x00) {
		if (rom_data[12] != 0 || rom_data[13] != 0 || rom_data[14] != 0 || rom_data[15] != 0) {
			printf("Error: Rom Header is not in iNES or NES 2.0 format, or is corrupted.\n");
			free(Cart);
			return NULL;
		}
		Cart->header_type = HEADER_INES;
		Cart->prg_ram_blocks = rom_data[8];
	}

	// If trainer code exists, store it, otherwise just zero out that block
	if (Cart->has_trainer) { memcpy(Cart->trainer, rom_data+16, TRAINER_BLOCK_SIZE); }
	else { memset(Cart->trainer, 0, TRAINER_BLOCK_SIZE); }

	// Make buffers to store the PRG ROM and CHR ROM blocks, aborting if there is not enough memory to do so
	Cart->prg_rom = malloc(PRG_ROM_BLOCK_SIZE * Cart->prg_rom_blocks);
	if (Cart->prg_rom == NULL) {
		printf("Error: Could not create cartridge object. Could not create PRG ROM buffer. Out of memory?\n");
		free(Cart);
		return 0;
	}
	Cart->chr_rom = malloc(CHR_ROM_BLOCK_SIZE * Cart->chr_rom_blocks);
	if (Cart->chr_rom == NULL) {
		printf("Error: Could not create cartridge object. Could not create PRG ROM buffer. Out of memory?\n");
		free(Cart->prg_rom);
		free(Cart);
		return 0;
	}

	// Copy the PRG ROM and CHR ROM blocks to the cartridge object
	memcpy(Cart->prg_rom, &rom_data[16] + ((TRAINER_BLOCK_SIZE * Cart->has_trainer) ? 1 : 0), PRG_ROM_BLOCK_SIZE * Cart->prg_rom_blocks);
	memcpy(Cart->chr_rom, &rom_data[16] + ((TRAINER_BLOCK_SIZE * Cart->has_trainer) ? 1 : 0) + PRG_ROM_BLOCK_SIZE * Cart->prg_rom_blocks, CHR_ROM_BLOCK_SIZE * Cart->chr_rom_blocks);
	// TO DO: Mapper 355 and 086 use Misc. ROM area following CHR ROM.

	return Cart;
}

uint8_t * NF_readROMtoBuffer(const char* filename) {
	FILE* fileptr;
	char* buffer;
	long filelen;
	fileptr = fopen(filename, "rb");
	fseek(fileptr, 0, SEEK_END);
	filelen = ftell(fileptr);
	rewind(fileptr);
	buffer = (uint8_t*)malloc(filelen * sizeof(uint8_t));
	fread(buffer, filelen, 1, fileptr);
	fclose(fileptr);
	return buffer;
}

uint8_t NF_readCartPRG_ROM(struct Cartridge *c, uint16_t address) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to read PRG ROM function.\n");
		return 0;
	}

	// Mapper 0
	// If there's 32 KB of prg_rom, it uses the whole address space. Otherwise, if there's 16KB, it's mirrored
	if (c->prg_rom_blocks == 2) { return c->prg_rom[address - 0x8000]; }
	else { return c->prg_rom[address - 0xC000]; }
}

uint8_t NF_readCartCHR_ROM(struct Cartridge* c, uint16_t address) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to read CHR ROM function.\n");
		return 0;
	}

	// Mapper 0
	return c->chr_rom[address];
}