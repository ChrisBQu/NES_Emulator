#ifndef NF_H_BUS
#define NF_H_BUS
#include "NF_Cartridge.h"
#include "NF_Palette.h"
#include <stdint.h>

// Representation of the memory. This maps in the following way:
// 
// $0000-07FF: 2KB Internal RAM [Zero page is $0000-$00FF, Stack is $0100-$01FF]
// $0800-1FFF: Mirrors of $0000-07FF [$0800-$0FFF], [$1000-$17FF], [$1800-$1FFF]
// $2000-2007: PPU Registers
// $2008-3FFF: Mirrors of $2000-2007 (Repeating every 8 bytes)
// $4000-4017: APU and I/O Ports
// $4018-401F: --- Usually disabled, used for CPU test mode ---
// $4020-5FFF: Cartridge expansion rom (Rarely used by mappers. MMC5 puts ram in there)
// $6000-7FFF: 8KB Cartridge RAM (Battery Backed Save, Work RAM)
// $8000-FFFA: 32KB Cartridge ROM
// $FFFA-FFFB: NMI Vector
// $FFFC-FFFD: Reset Vector
// $FFFE-FFFF: IRQ Vector

#define NF_6502_STACK_LOCATION (uint16_t)0x0100
#define NF_6502_ROM_LOCATION (uint16_t)0x8000
#define NF_6502_NMI_VECTOR (uint16_t)0xFFFA
#define NF_6502_RESET_VECTOR (uint16_t)0xFFFC
#define NF_6502_IRQ_VECTOR (uint16_t)0xFFFE

// This structure represents the console itself. It bundles objects making up the physical parts of the
// console, and acts as a bus, allowing them to communicate with one another
struct NES_Console {
	uint8_t Memory[0x10000];
	struct Cartridge* ConnectedCartridge;
	struct Processor* ConnectedProcessor;
	struct PictureProcessingUnit* ConnectedPPU;
	void (*imageOutFunc)(struct NF_Pixel);
};

// Must be called once to create the Console object
struct NES_Console* NF_initConsole();

// Connect a cartridge to the console. This function also places the Program Counter at the Reset vector
int NF_insertCartridge(struct NES_Console* console, struct Cartridge* cart);

// Send out one clock tick. This will advance both the CPU and the PPU appropriately
void NF_busTickMasterClock(struct NES_Console* console, bool r);

// Call the NMI function from the processor (this exists so that the PPU can send a signal to trigger it without being exposed to the CPU directly)
void NF_emitNMI(struct NES_Console* console);

// Write to the CPU memory address
void NF_writeMemory(struct NES_Console* console, uint16_t address, uint8_t value);

// Read from the CPU memory address
uint8_t NF_readMemory(struct NES_Console* console, uint16_t address);

#endif