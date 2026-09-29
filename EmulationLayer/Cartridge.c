#define _CRT_SECURE_NO_WARNINGS

#include "Cartridge.h"
#include "../Mappers/Mapper.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define PRG_ROM_BLOCK_SIZE 16384
#define CHR_ROM_BLOCK_SIZE 8192
#define TRAINER_BLOCK_SIZE 512
#define PRG_RAM_SIZE 8192

// Take byte data stored in a character array, and parse the ROM into a Cartridge structure
struct Cartridge * NES_createCartridgeFromBuffer(char* rom_data) {

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
	Cart->nametable_mirroring = (rom_data[6] & 0b00000001) ? VERTICAL_MAPPING : HORIZONTAL_MAPPING;
	Cart->flag_7 = rom_data[7];
	Cart->has_battery = ((rom_data[6] & 0b00000010) != 0);
	Cart->has_trainer = ((rom_data[6] & 0b00000100) != 0);
	Cart->mapper = (rom_data[6] >> 4) | (rom_data[7] & 0xF0);

	// Check if the header is NES 2.0
	if ((rom_data[7] & 0x0C) == 0x08) {
		Cart->header_type = HEADER_NES_2;
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
	// A CHR ROM size of 0 means the board has 8KB of CHR RAM instead, which the PPU can write to
	Cart->chr_rom = malloc(CHR_ROM_BLOCK_SIZE * (Cart->chr_rom_blocks == 0 ? 1 : Cart->chr_rom_blocks));
	if (Cart->chr_rom == NULL) {
		printf("Error: Could not create cartridge object. Could not create PRG ROM buffer. Out of memory?\n");
		free(Cart->prg_rom);
		free(Cart);
		return 0;
	}

	// Every board gets 8KB of PRG RAM. Boards without any would read open bus there, which games don't rely on
	Cart->prg_ram = calloc(PRG_RAM_SIZE, 1);
	if (Cart->prg_ram == NULL) {
		printf("Error: Could not create cartridge object. Could not create PRG RAM buffer. Out of memory?\n");
		free(Cart->chr_rom);
		free(Cart->prg_rom);
		free(Cart);
		return 0;
	}

	// Copy the PRG ROM and CHR ROM blocks to the cartridge object
	memcpy(Cart->prg_rom, &rom_data[16 + (Cart->has_trainer ? TRAINER_BLOCK_SIZE : 0)], PRG_ROM_BLOCK_SIZE * Cart->prg_rom_blocks);
	if (Cart->chr_rom_blocks == 0) { memset(Cart->chr_rom, 0, CHR_ROM_BLOCK_SIZE); }
	else { memcpy(Cart->chr_rom, &rom_data[16 + (Cart->has_trainer ? TRAINER_BLOCK_SIZE : 0) + PRG_ROM_BLOCK_SIZE * Cart->prg_rom_blocks], CHR_ROM_BLOCK_SIZE * Cart->chr_rom_blocks); }

	Cart->mapper_state = NULL;
	Cart->irq_asserted = false;
	if (Cart->mapper >= MAPPER_COUNT) {
		printf("Error: Mapper %d is not supported.\n", Cart->mapper);
		free(Cart->prg_ram);
		free(Cart->chr_rom);
		free(Cart->prg_rom);
		free(Cart);
		return NULL;
	}
	if (!MapperList[Cart->mapper].init(Cart)) {
		free(Cart->prg_ram);
		free(Cart->chr_rom);
		free(Cart->prg_rom);
		free(Cart);
		return NULL;
	}

	return Cart;
}

// Helper function, read contents of ROM file into character array
uint8_t * NES_readROMtoBuffer(const char* filename) {
	FILE* fileptr;
	char* buffer;
	long filelen;
	fileptr = fopen(filename, "rb");
	if (fileptr == NULL) {
		printf("Error: Could not open ROM file '%s'.\n", filename);
		return NULL;
	}
	fseek(fileptr, 0, SEEK_END);
	filelen = ftell(fileptr);
	rewind(fileptr);
	buffer = (uint8_t*)malloc(filelen * sizeof(uint8_t));
	if (buffer == NULL) {
		printf("Error: Could not allocate a buffer for the ROM file. Out of memory?\n");
		fclose(fileptr);
		return NULL;
	}
	fread(buffer, filelen, 1, fileptr);
	fclose(fileptr);
	return buffer;
}

uint8_t NES_readCartPRG_ROM(struct Cartridge *c, uint16_t address) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to read PRG ROM function.\n");
		return 0;
	}

	if (c->mapper < MAPPER_COUNT) {
		return MapperList[c->mapper].readPRG_ROM(c, address);
	}
	else {
		printf("Error: Mapper %d is not supported, so this game will likely not run correctly.\n", c->mapper);
		return 0;
	}
}

void NES_writeCartPRG(struct Cartridge* c, uint16_t address, uint8_t data, uint64_t cpu_cycle) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to write PRG function.\n");
		return;
	}

	if (c->mapper < MAPPER_COUNT) {
		MapperList[c->mapper].writePRG(c, address, data, cpu_cycle);
	}
	else {
		printf("Error: Mapper %d is not supported, so this game will likely not run correctly.\n", c->mapper);
	}
}

uint8_t NES_readCartPRG_RAM(struct Cartridge* c, uint16_t address) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to read PRG RAM function.\n");
		return 0;
	}

	if (c->mapper < MAPPER_COUNT) {
		return MapperList[c->mapper].readPRG_RAM(c, address);
	}
	else {
		printf("Error: Mapper %d is not supported, so this game will likely not run correctly.\n", c->mapper);
		return 0;
	}
}

void NES_writeCartPRG_RAM(struct Cartridge* c, uint16_t address, uint8_t data) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to write PRG RAM function.\n");
		return;
	}

	if (c->mapper < MAPPER_COUNT) {
		MapperList[c->mapper].writePRG_RAM(c, address, data);
	}
	else {
		printf("Error: Mapper %d is not supported, so this game will likely not run correctly.\n", c->mapper);
	}
}

uint8_t NES_readCartCHR_ROM(struct Cartridge* c, uint16_t address) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to read CHR ROM function.\n");
		return 0;
	}

	if (c->mapper < MAPPER_COUNT) {
		return MapperList[c->mapper].readCHR_ROM(c, address);
	}
	else {
		printf("Error: Mapper %d is not supported, so this game will likely not run correctly.\n", c->mapper);
		return 0;
	}
}

void NES_writeCartCHR(struct Cartridge* c, uint16_t address, uint8_t data) {

	if (c == NULL) {
		printf("Error: There is no cartridge connected to the bus, or no cartridge was passed to write CHR function.\n");
		return;
	}
	if (c->mapper < MAPPER_COUNT) {
		MapperList[c->mapper].writeCHR(c, address, data);
		return;
	}
	else {
		printf("Error: Mapper %d is not supported, so this game will likely not run correctly.\n", c->mapper);
	}
}

// Some mappers, such as MMC3, watch the PPU address bus to count scanlines. For such carts, pass along every address the PPU drives
void NES_notifyCartPPUAddress(struct Cartridge* c, uint16_t address, uint64_t cpu_cycle) {
	if (c == NULL || c->mapper >= MAPPER_COUNT) { return; }
	if (MapperList[c->mapper].notifyPPUAddress != NULL) { MapperList[c->mapper].notifyPPUAddress(c, address, cpu_cycle); }
}

void NES_notifyCartPPURegisterWrite(struct Cartridge* c, uint16_t address, uint8_t data) {
	if (c == NULL || c->mapper >= MAPPER_COUNT) { return; }
	if (MapperList[c->mapper].notifyPPURegisterWrite != NULL) { MapperList[c->mapper].notifyPPURegisterWrite(c, address, data); }
}

uint8_t NES_readCartExpansion(struct Cartridge* c, uint16_t address) {
	// Nothing drives the bus here on most boards, so reads return open bus. The last byte on the bus is usually the high byte of the address
	if (c == NULL || c->mapper >= MAPPER_COUNT || MapperList[c->mapper].readExpansion == NULL) { return address >> 8; }
	return MapperList[c->mapper].readExpansion(c, address);
}

void NES_writeCartExpansion(struct Cartridge* c, uint16_t address, uint8_t data) {
	if (c == NULL || c->mapper >= MAPPER_COUNT) { return; }
	if (MapperList[c->mapper].writeExpansion != NULL) { MapperList[c->mapper].writeExpansion(c, address, data); }
}

bool NES_cartMapsNametables(struct Cartridge* c) {
	return c != NULL && c->mapper < MAPPER_COUNT && MapperList[c->mapper].readNametable != NULL;
}

uint8_t NES_readCartNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram) {
	return MapperList[c->mapper].readNametable(c, offset, ciram);
}

void NES_writeCartNametable(struct Cartridge* c, uint16_t offset, uint8_t* ciram, uint8_t data) {
	MapperList[c->mapper].writeNametable(c, offset, ciram, data);
}

void NES_tickCart(struct Cartridge* c) {
	if (c == NULL || c->mapper >= MAPPER_COUNT) { return; }
	if (MapperList[c->mapper].tick != NULL) { MapperList[c->mapper].tick(c); }
}

void NES_freeCartridge(struct Cartridge* c) {
	if (c == NULL) { return; }
	if (c->prg_rom != NULL) { free(c->prg_rom); }
	if (c->chr_rom != NULL) { free(c->chr_rom); }
	if (c->prg_ram != NULL) { free(c->prg_ram); }
	if (c->mapper_state != NULL) { free(c->mapper_state); }
	free(c);
}
