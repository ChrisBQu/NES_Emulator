#include "NF_Bus.h"
#include "NF_6502.h"
#include "NF_PPU.h"
#include "NF_APU.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	console->ConnectedAPU = NF_initAPU();
	if (console->ConnectedAPU == NULL) { return 0; }
	console->ConnectedAPU->bus = console;

	console->ConnectedControllerPort = NF_initControllerPort();
	if (console->ConnectedControllerPort == NULL) { return 0; }
	console->ConnectedControllerPort->bus = console;

	console->ConnectedCartridge = NULL;
	console->imageOutFunc = NULL;
	console->audioOutFunc = NULL;
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
	//console->ConnectedProcessor->PC = 0xC000; // For testing with nestest.nes, comment out otherwise
	return 0;
}

void NF_writeMemory(struct NES_Console* console, uint16_t address, uint8_t value) {

	// Addresses in the 2KB internal RAM should be mirrored onto [0x0000 - 0x07FF] if they are outside of that range
	if (address <= 0x1FFF) { console->Memory[address % 0x800] = value; }

	// PPU register addresses are also mirrored repeatedly
	// Some mappers (MMC5) also watch these writes
	else if (address >= 0x2000 && address <= 0x3FFF) {
		NF_PPU_writeRegister(console->ConnectedPPU, (PPU_REGISTER)(address % 0x08), value);
		NF_notifyCartPPURegisterWrite(console->ConnectedCartridge, address, value);
	}

	// OAM DMA: copy the 256-byte page $XX00-$XXFF into the PPU's OAM. The CPU is halted while this happens
	else if (address == 0x4014) {
		uint8_t page[256];
		for (int i = 0; i < 256; i++) {
			page[i] = NF_readMemory(console, (uint16_t)((value << 8) | i));
		}
		NF_PPU_writeOAMDMA(console->ConnectedPPU, page);
		console->ConnectedProcessor->cycles += 513;
	}

	// Controller strobe: while bit 0 is set, both controllers keep latching their current buttons
	else if (address == 0x4016) {
		console->ConnectedControllerPort->controller_strobe = (value & 0x01) != 0;
		if (console->ConnectedControllerPort->controller_strobe) {
			console->ConnectedControllerPort->controller_shift[0] = console->ConnectedControllerPort->controller_state[0];
			console->ConnectedControllerPort->controller_shift[1] = console->ConnectedControllerPort->controller_state[1];
		}
	}

	// APU registers ($4017 is the frame counter when written, but controller 2 when read)
	else if ((address >= 0x4000 && address <= 0x4013) || address == 0x4015 || address == 0x4017) {
		NF_APU_writeRegister(console->ConnectedAPU, address, value);
	}

	// Cartridge expansion area, where some mappers (MMC5) have registers and RAM
	else if (address >= 0x4020 && address < 0x6000) {
		NF_writeCartExpansion(console->ConnectedCartridge, address, value);
	}

	// PRG RAM on the cartridge
	else if (address >= 0x6000 && address < NF_6502_ROM_LOCATION) {
		NF_writeCartPRG_RAM(console->ConnectedCartridge, address, value);
	}

	// ROM itself can't be written, but mappers watch writes to this range to set their registers
	else if (address >= NF_6502_ROM_LOCATION) {
		NF_writeCartPRG(console->ConnectedCartridge, address, value, console->ConnectedProcessor->cycle_count);
	}

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

	// APU status is the only readable APU register
	else if (address == 0x4015) {
		return NF_APU_readStatus(console->ConnectedAPU);
	}

	// Controllers return one button per read, in bit 0. Bit 6 reads as 1 from open bus
	else if (address == 0x4016 || address == 0x4017) {
		int port = address - 0x4016;
		if (console->ConnectedControllerPort->controller_strobe) { console->ConnectedControllerPort->controller_shift[port] = console->ConnectedControllerPort->controller_state[port]; }
		uint8_t bit = console->ConnectedControllerPort->controller_shift[port] & 0x01;
		console->ConnectedControllerPort->controller_shift[port] = (console->ConnectedControllerPort->controller_shift[port] >> 1) | 0x80;  // After 8 reads, a real controller returns 1s
		return 0x40 | bit;
	}

	// Cartridge expansion area, where some mappers (MMC5) have registers and RAM
	else if (address >= 0x4020 && address < 0x6000) {
		return NF_readCartExpansion(console->ConnectedCartridge, address);
	}

	// PRG RAM on the cartridge
	else if (address >= 0x6000 && address < NF_6502_ROM_LOCATION) {
		return NF_readCartPRG_RAM(console->ConnectedCartridge, address);
	}

	// Reading PRG Rom from the cartridge
	else if (address >= NF_6502_ROM_LOCATION) {
		return NF_readCartPRG_ROM(console->ConnectedCartridge, address);
	}

	else { return console->Memory[address]; }
}

// Read from the CPU address space without triggering side effects. Reading some PPU registers changes PPU state
// (e.g. PPUSTATUS clears VBlank, PPUDATA advances the VRAM address), so this is used for dummy operand fetches and the debugger
uint8_t NF_peekMemory(struct NES_Console* console, uint16_t address) {
	if (address >= 0x2000 && address <= 0x3FFF) { return 0x00; }
	if (address == 0x4015) { return 0x00; }  // Reading APU status clears the frame IRQ flag
	if (address == 0x4016 || address == 0x4017) { return 0x40 | (console->ConnectedControllerPort->controller_shift[address - 0x4016] & 0x01); }  // Reading shifts the controller
	if (address >= 0x4020 && address < 0x6000) { return 0x00; }  // Mapper registers here can have read side effects (MMC5's $5204 acknowledges its IRQ)
	return NF_readMemory(console, address);
}

// Signals the NMI line of the connected Processor, which will service it once the current instruction finishes.
// This exists so that the PPU can trigger the NMI by passing up a signal through the bus that it is on (VBlank)
void NF_emitNMI(struct NES_Console* console) {
	console->ConnectedProcessor->nmi_pending = true;
}

//When the IRQ line is level triggered: it stays asserted until whatever pulled it low (currently only the cartridge) acknowledges it
bool NF_isIRQAsserted(struct NES_Console* console) {
	return console->ConnectedCartridge != NULL && console->ConnectedCartridge->irq_asserted;
}

// The NES uses a single master clock, and for every 3 ticks of the PPU, the CPU has one tick
// The startup_ready flag is used to skip the startup cycles
void NF_busTickMasterClock(struct NES_Console* console, bool startup_ready) {
	if (startup_ready) {
		NF_6502_tickClock(console->ConnectedProcessor);
		NF_APU_tickClock(console->ConnectedAPU);
		NF_tickCart(console->ConnectedCartridge);
		NF_PPU_tickClock(console->ConnectedPPU);
		NF_PPU_tickClock(console->ConnectedPPU);
		NF_PPU_tickClock(console->ConnectedPPU);
	}
}
