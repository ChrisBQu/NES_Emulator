#ifndef NF_H_6502
#define NF_H_6502
#include "NF_Bus.h"
#include <stdint.h>
#include <stdbool.h>

// A list of all address modes on the 6502
typedef enum {
	AM_ACC,         // Accumulator
	AM_IMM,			// Immediate
	AM_REL,			// Relative
	AM_IMP,			// Implied
	AM_ZPG,			// Zero Page
	AM_ZPX,			// Zero Page, X
	AM_ZPY,			// Zero Page, Y
	AM_ABS,			// Absolute
	AM_ABX,			// Absolute, X
	AM_ABY,			// Absolute, Y
	AM_IND,			// Indirect
	AM_INX,			// Indirect, X
	AM_INY,			// Indirect, Y
	AM_XXX,			// Unknown (Paired with illegal opcodes that are not supported)
} ADDRESS_MODE_6502;

// A list of all legal opcodes on the 6502
typedef enum {
	OP_ADC, OP_AND, OP_ASL, OP_BCC, OP_BCS, OP_BEQ, OP_BIT, OP_BMI, OP_BNE, OP_BPL, OP_BRK, OP_BVC, OP_BVS, OP_CLC,
	OP_CLD, OP_CLI, OP_CLV, OP_CMP, OP_CPX, OP_CPY, OP_DEC, OP_DEX, OP_DEY, OP_EOR, OP_INC, OP_INX, OP_INY, OP_JMP,
	OP_JSR, OP_LDA, OP_LDX, OP_LDY, OP_LSR, OP_NOP, OP_ORA, OP_PHA, OP_PHP, OP_PLA, OP_PLP, OP_ROL, OP_ROR, OP_RTI,
	OP_RTS, OP_SBC, OP_SEC, OP_SED, OP_SEI, OP_STA, OP_STX, OP_STY, OP_TAX, OP_TAY, OP_TSX, OP_TXA, OP_TXS, OP_TYA,
	OP_XXX
} OPCODE_6502;



OPCODE_6502 charToOpcodeArray[256];
ADDRESS_MODE_6502 charToAddressModeArray[256];

// A struct to represent the Processor
struct Processor {
	// Variables representing the hardware
	uint16_t PC;					// Program counter
	uint8_t A;						// Accumulator
	uint8_t X;						// X-register
	uint8_t Y;						// Y-register
	uint8_t SP;						// SP is the Stack Pointer
	uint8_t P;						// P is the Processor Status register
	// Variables that will help in emulating its functionality
	uint8_t cycles;					// Number of cycles needed to finish performing the operation being executed
	OPCODE_6502 opcode;			    // Opcode currently being executed
	ADDRESS_MODE_6502 addr_mode;    // Address mode being used by the current opcode
	uint8_t fetched;
	uint16_t fetched_address;
	struct NES_Console* bus;
	bool page_crossed;

	// Debugger values
	uint16_t last_pc;

};

typedef enum {
	FLAG_C = 0b00000001, // Carry
	FLAG_Z = 0b00000010, // Zero
	FLAG_I = 0b00000100, // Disable Interrupts
	FLAG_D = 0b00001000, // Decimal Mode (unsupported by the NES, although the flag can still be cleared/set)
	FLAG_B = 0b00010000, // Break
	FLAG_U = 0b00100000, // Unused
	FLAG_V = 0b01000000, // Overflow
	FLAG_N = 0b10000000, // Negative
} FLAG_6502;

struct Processor * NF_6502_initProcessor();
void NF_6502_tickClock(struct Processor *CPU);
void NF_6502_reset(struct Processor* CPU);
void NF_6502_irq(struct Processor* CPU);
void NF_6502_nmi(struct Processor* CPU);
uint8_t NF_6502_getFlag(struct Processor *CPU, FLAG_6502 flag);

#endif