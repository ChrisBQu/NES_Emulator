#include "NF_Bus.h"
#include "NF_6502.h"
#include "NF_PPU.h"
#include <stdio.h>
#include <stdlib.h>

// Constructor
struct NES_Console* NF_initConsole() {
	struct NES_Console* console = malloc(sizeof(struct NES_Console));
	if (console == NULL) {
		printf("Error: Could not create NES Console object. Out of memory?\n");
		return 0;
	}

	console->ConnectedProcessor = NF_6502_initProcessor();
	if (console->ConnectedProcessor == NULL) { return 0; }
	console->ConnectedProcessor->bus = console;

	console->ConnectedPPU = NF_initPPU();
	if (console->ConnectedPPU == NULL) { return 0; }
	console->ConnectedPPU->bus = console;

	memset(console->Memory, 0, 0x10000);
	return console;
}

// Connect cartridge to the BUS, which will enable memory reading. Also adjust the program counter to the start of code from the cartridge
int NF_insertCartridge(struct NES_Console *console, struct Cartridge *cart) {
	if (cart == NULL) { 
		printf("Error: The cartridge connected to the bus is a null pointer.\n");
		return 1;
	}
	console->ConnectedCartridge = cart; 
	console->ConnectedProcessor->PC = (NF_readMemory(console, NF_6502_RESET_VECTOR + 1) << 8) | NF_readMemory(console, NF_6502_RESET_VECTOR);
	console->ConnectedProcessor->PC = 0xC000; // For testing with nestest.nes, comment out otherwise
}

void NF_writeMemory(struct NES_Console* console, uint16_t address, uint8_t value) {

	// Addresses in the 2KB internal RAM should be mirrored onto [0x0000 - 0x07FF] if they are outside of that range
	if (address <= 0x1FFF) { console->Memory[address % 0x800] = value; }

	// PPU register addresses are also mirrored repeatedly
	else if (address >= 0x2000 && address <= 0x3FFF) {
		NF_PPU_writeRegister(console->ConnectedPPU, (PPU_REGISTER)(address % 0x08), value);
	}

	// Reading PRG Rom from the cartridge
	else if (address >= NF_6502_ROM_LOCATION) { /* Cannot do anything to ROM */ }

	else { console->Memory[address] = value; }
}

uint8_t NF_readMemory(struct NES_Console* console, uint16_t address) {

	if (address < 0x0000 || address > 0xFFFF) {
		printf("Error: Address range for memory reads must be between 0x0000 and 0xFFFF.\n");
		return 0;
	}

	// Addresses in the 2KB internal RAM should be mirrored onto [0x0000 - 0x07FF] if they are outside of that range
	else if (address <= 0x1FFF) { return console->Memory[address % 0x800]; }

	// The eight PPU register addresses are also mirrored repeatedly, so we use modulo division to get which register it is
	else if (address >= 0x2000 && address <= 0x3FFF) { 
		return NF_PPU_readRegister(console->ConnectedPPU, (PPU_REGISTER)(address % 0x08));
	}

	// Reading PRG Rom from the cartridge
	else if (address >= NF_6502_ROM_LOCATION) {
		return NF_readCartPRG_ROM(console->ConnectedCartridge, address);
	}

	else { return console->Memory[address]; }
}

// Calls the NMI function of the connected Processor. 
// This exists so that the PPU can trigger the NMI by passing up a signal through the bus that it is on (VBlank)
void NF_emitNMI(struct NES_Console* console) {
	NF_6502_nmi(console->ConnectedProcessor);
}

// The NES uses a single master clock, and for every 3 ticks of the PPU, the CPU has one tick
void NF_busTickMasterClock(struct NES_Console* console, bool r) {
	NF_6502_tickClock(console->ConnectedProcessor);
	NF_PPU_tickClock(console->ConnectedPPU);
	NF_PPU_tickClock(console->ConnectedPPU);
	NF_PPU_tickClock(console->ConnectedPPU);
}
