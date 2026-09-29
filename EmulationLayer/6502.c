#include "6502.h"
#include "Bus.h"
#include "PPU.h"
#include "Debugger.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Debug log
FILE* myLog;

// Total CPU cycles
int total_cycles = 7;

// It is important to be able to convert an opcode (in range 0x00 to 0xff) to an opcode and an addressing mode.
// To aid in this, we have two arrays, one which contains the opcodes, and one which contains the address modes.
// This is the compromise between code that is readable, code that runs fast (accessible in constant time), and code that does not use
// too much memory.
OPCODE_6502 charToOpcodeArray[256] =
{
	//  0       1       2       3       4       5       6       7       8       9      A        B       C       D       E       F  
	OP_BRK, OP_ORA, OP_XXX, OP_XXX, OP_XXX, OP_ORA, OP_ASL, OP_XXX, OP_PHP, OP_ORA, OP_ASL, OP_XXX, OP_XXX, OP_ORA, OP_ASL, OP_XXX, // 0 (0)
	OP_BPL, OP_ORA, OP_XXX, OP_XXX, OP_XXX, OP_ORA, OP_ASL, OP_XXX, OP_CLC, OP_ORA, OP_XXX, OP_XXX, OP_XXX, OP_ORA, OP_ASL, OP_XXX, // 1 (16)
	OP_JSR, OP_AND, OP_XXX, OP_XXX, OP_BIT, OP_AND, OP_ROL, OP_XXX, OP_PLP, OP_AND, OP_ROL, OP_XXX, OP_BIT, OP_AND, OP_ROL, OP_XXX, // 2 (32)
	OP_BMI, OP_AND, OP_XXX, OP_XXX, OP_XXX, OP_AND, OP_ROL, OP_XXX, OP_SEC, OP_AND, OP_XXX, OP_XXX, OP_XXX, OP_AND, OP_ROL, OP_XXX, // 3 (48)
	OP_RTI, OP_EOR, OP_XXX, OP_XXX, OP_XXX, OP_EOR, OP_LSR, OP_XXX, OP_PHA, OP_EOR, OP_LSR, OP_XXX, OP_JMP, OP_EOR, OP_LSR, OP_XXX, // 4 (64)
	OP_BVC, OP_EOR, OP_XXX, OP_XXX, OP_XXX, OP_EOR, OP_LSR, OP_XXX, OP_CLI, OP_EOR, OP_XXX, OP_XXX, OP_XXX, OP_EOR, OP_LSR, OP_XXX, // 5 (80)
	OP_RTS, OP_ADC, OP_XXX, OP_XXX, OP_XXX, OP_ADC, OP_ROR, OP_XXX, OP_PLA, OP_ADC, OP_ROR, OP_XXX, OP_JMP, OP_ADC, OP_ROR, OP_XXX, // 6 (96)
	OP_BVS, OP_ADC, OP_XXX, OP_XXX, OP_XXX, OP_ADC, OP_ROR, OP_XXX, OP_SEI, OP_ADC, OP_XXX, OP_XXX, OP_XXX, OP_ADC, OP_ROR, OP_XXX, // 7 (112)
	OP_XXX, OP_STA, OP_XXX, OP_XXX, OP_STY, OP_STA, OP_STX, OP_XXX, OP_DEY, OP_XXX, OP_TXA, OP_XXX, OP_STY, OP_STA, OP_STX, OP_XXX, // 8 (128)
	OP_BCC, OP_STA, OP_XXX, OP_XXX, OP_STY, OP_STA, OP_STX, OP_XXX, OP_TYA, OP_STA, OP_TXS, OP_XXX, OP_XXX, OP_STA, OP_XXX, OP_XXX, // 9 (144)
	OP_LDY, OP_LDA, OP_LDX, OP_XXX, OP_LDY, OP_LDA, OP_LDX, OP_XXX, OP_TAY, OP_LDA, OP_TAX, OP_XXX, OP_LDY, OP_LDA, OP_LDX, OP_XXX, // A (160)
	OP_BCS, OP_LDA, OP_XXX, OP_XXX, OP_LDY, OP_LDA, OP_LDX, OP_XXX, OP_CLV, OP_LDA, OP_TSX, OP_XXX, OP_LDY, OP_LDA, OP_LDX, OP_XXX, // B (176)
	OP_CPY, OP_CMP, OP_XXX, OP_XXX, OP_CPY, OP_CMP, OP_DEC, OP_XXX, OP_INY, OP_CMP, OP_DEX, OP_XXX, OP_CPY, OP_CMP, OP_DEC, OP_XXX, // C (192)
	OP_BNE, OP_CMP, OP_XXX, OP_XXX, OP_XXX, OP_CMP, OP_DEC, OP_XXX, OP_CLD, OP_CMP, OP_XXX, OP_XXX, OP_XXX, OP_CMP, OP_DEC, OP_XXX, // D (208)
	OP_CPX, OP_SBC, OP_XXX, OP_XXX, OP_CPX, OP_SBC, OP_INC, OP_XXX, OP_INX, OP_SBC, OP_NOP, OP_XXX, OP_CPX, OP_SBC, OP_INC, OP_XXX, // E (224)
	OP_BEQ, OP_SBC, OP_XXX, OP_XXX, OP_XXX, OP_SBC, OP_INC, OP_XXX, OP_SED, OP_SBC, OP_XXX, OP_XXX, OP_XXX, OP_SBC, OP_INC, OP_XXX  // F (240)
};

ADDRESS_MODE_6502 charToAddressModeArray[256] =
{
	//  0       1       2       3       4       5       6       7       8       9      A        B       C       D       E       F  
	AM_IMP, AM_INX, AM_XXX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_ACC, AM_XXX, AM_XXX, AM_ABS, AM_ABS, AM_XXX, // 0
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_XXX, AM_IMP, AM_ABY, AM_XXX, AM_XXX, AM_XXX, AM_ABX, AM_ABX, AM_XXX, // 1
	AM_ABS, AM_INX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_ACC, AM_XXX, AM_ABS, AM_ABS, AM_ABS, AM_XXX, // 2
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_XXX, AM_IMP, AM_ABY, AM_XXX, AM_XXX, AM_XXX, AM_ABX, AM_ABX, AM_XXX, // 3
	AM_IMP, AM_INX, AM_XXX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_ACC, AM_XXX, AM_ABS, AM_ABS, AM_ABS, AM_XXX, // 4
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_XXX, AM_IMP, AM_ABY, AM_XXX, AM_XXX, AM_XXX, AM_ABX, AM_ABX, AM_XXX, // 5
	AM_IMP, AM_INX, AM_XXX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_ACC, AM_XXX, AM_IND, AM_ABS, AM_ABS, AM_XXX, // 6
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_XXX, AM_IMP, AM_ABY, AM_XXX, AM_XXX, AM_XXX, AM_ABX, AM_ABX, AM_XXX, // 7
	AM_XXX, AM_INX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_XXX, AM_IMP, AM_XXX, AM_ABS, AM_ABS, AM_ABS, AM_XXX, // 8
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_ZPY, AM_XXX, AM_IMP, AM_ABY, AM_IMP, AM_XXX, AM_XXX, AM_ABX, AM_XXX, AM_XXX, // 9
	AM_IMM, AM_INX, AM_IMM, AM_XXX, AM_ZPG, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_IMP, AM_XXX, AM_ABS, AM_ABS, AM_ABS, AM_XXX, // A
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_ZPY, AM_XXX, AM_IMP, AM_ABY, AM_IMP, AM_XXX, AM_ABX, AM_ABX, AM_ABY, AM_XXX, // B
	AM_IMM, AM_INX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_IMP, AM_XXX, AM_ABS, AM_ABS, AM_ABS, AM_XXX, // C
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_XXX, AM_IMP, AM_ABY, AM_XXX, AM_XXX, AM_XXX, AM_ABX, AM_ABX, AM_XXX, // D
	AM_IMM, AM_INX, AM_XXX, AM_XXX, AM_ZPG, AM_ZPG, AM_ZPG, AM_XXX, AM_IMP, AM_IMM, AM_IMP, AM_XXX, AM_ABS, AM_ABS, AM_ABS, AM_XXX, // E
	AM_REL, AM_INY, AM_XXX, AM_XXX, AM_XXX, AM_ZPX, AM_ZPX, AM_XXX, AM_IMP, AM_ABY, AM_XXX, AM_XXX, AM_XXX, AM_ABX, AM_ABX, AM_XXX  // F
};

// An array holding the number of cycles required by each opcode/address mode pair.
// A zero indicates that the opcode or address mode is illegal and unsupported by this implementation
uint8_t cyclesArray[256] =
{
 // 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	7, 6, 0, 0, 3, 3, 5, 0, 3, 2, 2, 0, 0, 4, 6, 0, // 0
	2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0, // 1
	6, 6, 0, 0, 3, 3, 5, 0, 4, 2, 2, 0, 4, 4, 6, 0, // 2
	2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0, // 3
	6, 6, 0, 0, 0, 3, 5, 0, 3, 2, 2, 0, 3, 4, 6, 0, // 4
	2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0, // 5
	6, 6, 0, 0, 0, 3, 5, 0, 4, 2, 2, 0, 5, 4, 6, 0, // 6
	2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0, // 7
	0, 6, 0, 0, 3, 3, 3, 0, 2, 0, 2, 0, 4, 4, 4, 0, // 8
	2, 6, 0, 0, 4, 4, 4, 0, 2, 5, 2, 0, 0, 5, 0, 0, // 9
	2, 6, 2, 0, 3, 3, 3, 0, 2, 2, 2, 0, 4, 4, 4, 0, // A
	2, 5, 0, 0, 4, 4, 4, 0, 2, 4, 2, 0, 4, 4, 4, 0, // B
	2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0, // C
	2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0, // D
	2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0, // E
	2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0  // F
};




// Initialize the CPU-> This must be called once before trying to use it
struct Processor* NES_6502_initProcessor() {

	// If debugging is enabled, open a file for logging
	if (DEBUG_ENABLED) { myLog = fopen("log.txt", "w"); }

	struct Processor* newcpu = malloc(sizeof(struct Processor));
	if (newcpu == NULL) {
		printf("Error: Could not create 6502 Processor object. Out of memory?\n");
		return 0;
	}

	newcpu->bus = NULL;
	NES_6502_resetState(newcpu);
	return newcpu;

}

// Put the CPU back into its power-on state. Unlike NES_6502_reset (the reset signal), this clears every register.
// The bus pointer is kept
void NES_6502_resetState(struct Processor* CPU) {
	CPU->PC = 0x00;
	CPU->A = 0x00;
	CPU->X = 0x00;
	CPU->Y = 0x00;
	CPU->SP = 0xfd;
	CPU->P = 0b00100100;
	CPU->cycles = 0;
	CPU->cycle_count = 7;		// Resetting takes 7 cycles
	CPU->page_crossed = false;
	CPU->nmi_pending = false;
	total_cycles = 7;
}

// Set one of the processor flags to either 0 or 1. Function exists as a convenience.
void NES_6502_setFlag(struct Processor* CPU, FLAG_6502 flag, bool value) {
	if (value) { CPU->P |= flag; }
	else { CPU->P &= ~flag; }
}

// Get one of the processor flags. Function exists as a convenience.
uint8_t NES_6502_getFlag(struct Processor* CPU, FLAG_6502 flag) {
	return (CPU->P & flag);
}

// Read the value at CPU->fetched_address. Store instructions never actually read their target, so for them this is
// side-effect free: otherwise e.g. STA $2007 would perform a PPUDATA read and advance the PPU's VRAM address twice.
static uint8_t NES_readOperand(struct Processor* CPU) {
	if (CPU->opcode == OP_STA || CPU->opcode == OP_STX || CPU->opcode == OP_STY) {
		return NES_peekMemory(CPU->bus, CPU->fetched_address);
	}
	return NES_readMemory(CPU->bus, CPU->fetched_address);
}

// Each opcode instruction is between 1 and 3 bytes. The first byte tells what the instruction is.
// Calling this function will then use the current address mode to store some data from the other bytes.
// What this data is, and where it is located will depend on the instruction. It could either be a literal,
// stored in the accumulator, or be located at a memory address. But it will always be a uint8_t. It
// will then be stored at CPU->fetched, and can be used during the execution of the opcode.
void NES_fetchData(struct Processor* CPU) {
	uint16_t tmp = 0x00;
	uint8_t tmp8 = 0x00;
	uint16_t lo = 0x00;
	uint16_t hi = 0x00;
	switch (CPU->addr_mode) {
	case AM_ACC:
		CPU->fetched = CPU->A;
		break;
	case AM_IMM:
		CPU->fetched = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		break;
	case AM_REL:
		CPU->fetched = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		if (CPU->fetched & 0x80) {
			tmp8 = ~CPU->fetched + 1;
			CPU->fetched_address = CPU->PC - tmp8;
		}
		else { CPU->fetched_address = CPU->PC + CPU->fetched; }
		break;
	case AM_IMP:
		CPU->fetched = CPU->A;
		break;
	case AM_ZPG:
		CPU->fetched_address = 0x00FF & NES_readMemory(CPU->bus, CPU->PC);
		CPU->fetched = NES_readOperand(CPU);
		CPU->PC++;
		break;
	case AM_ZPX:
		CPU->fetched_address = 0x00FF & (NES_readMemory(CPU->bus, CPU->PC) + CPU->X);
		CPU->fetched = NES_readOperand(CPU);
		CPU->PC++;
		break;
	case AM_ZPY:
		CPU->fetched_address = 0x00FF & (NES_readMemory(CPU->bus, CPU->PC) + CPU->Y);
		CPU->fetched = NES_readOperand(CPU);
		CPU->PC++;
		break;
	case AM_ABS:
		lo = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		hi = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		CPU->fetched_address = (hi << 8) | lo;
		CPU->fetched = NES_readOperand(CPU);
		break;
	case AM_ABX:
		lo = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		hi = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		CPU->fetched_address = ((hi << 8) | lo) + CPU->X;
		CPU->fetched = NES_readOperand(CPU);
		if ((CPU->fetched_address & 0xFF00) != (hi << 8)) { CPU->page_crossed = true; }
		break;
	case AM_ABY:
		lo = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		hi = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		CPU->fetched_address = ((hi << 8) | lo) + CPU->Y;
		CPU->fetched = NES_readOperand(CPU);
		if ((CPU->fetched_address & 0xFF00) != (hi << 8)) { CPU->page_crossed = true; }
		break;
	case AM_IND:
		lo = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		hi = NES_readMemory(CPU->bus, CPU->PC);
		CPU->PC++;
		tmp = (hi << 8) | lo;
		// The 6502 has a bug where if the hi byte crosses a page boundary above the lo byte, instead the hi
		// byte will be pulled from 00 of the same page. It wraps around to it.
		if (lo == 0x00FF) { CPU->fetched_address = ((NES_readMemory(CPU->bus, tmp & 0xFF00) << 8) | NES_readMemory(CPU->bus, tmp)); }
		else { CPU->fetched_address = ((NES_readMemory(CPU->bus, tmp + 1) << 8) | NES_readMemory(CPU->bus, tmp)); }
		CPU->fetched = NES_readOperand(CPU);
		break;
	case AM_INX:
		tmp = NES_readMemory(CPU->bus, CPU->PC) + CPU->X;
		CPU->PC++;
		lo = NES_readMemory(CPU->bus, tmp & 0x00FF);
		hi = NES_readMemory(CPU->bus, (tmp + 1) & 0x00FF);
		CPU->fetched_address = (hi << 8) | lo;
		CPU->fetched = NES_readOperand(CPU);
		if ((CPU->fetched_address & 0xFF00) != (hi << 8)) { CPU->page_crossed = true; }
		break;
	case AM_INY:
		tmp = (0x00FF & NES_readMemory(CPU->bus, CPU->PC));
		CPU->PC++;
		lo = NES_readMemory(CPU->bus, tmp & 0x00FF);
		hi = NES_readMemory(CPU->bus, (tmp + 1) & 0x00FF);
		CPU->fetched_address = ((hi << 8) | lo) + CPU->Y;
		CPU->fetched = NES_readOperand(CPU);
		if ((CPU->fetched_address & 0xFF00) != (hi << 8)) { CPU->page_crossed = true; }
		break;
	case AM_XXX:
		if (DEBUG_ENABLED && myLog != NULL) {
			fflush(myLog);
			fclose(myLog);
			myLog = NULL;
		}
		//printf("Error: An illegal addressing mode was used. No value fetched.\n");
		break;
	default:
		if (DEBUG_ENABLED && myLog != NULL) {
			fflush(myLog);
			fclose(myLog);
			myLog = NULL;
		}
		//printf("Error: A valid addressing mode was not passed. No value fetched.\n");
		break;
	}
}

void NES_executeInstruction(struct Processor* CPU) {

	uint16_t lo = 0x00;
	uint16_t hi = 0x00;
	uint16_t tmp;
	uint8_t tmp8;
	uint16_t partial;
	switch (CPU->opcode) {
	case OP_ADC:
		// Formula: A = A + Memory + Carry
		if (CPU->page_crossed) { CPU->cycles++; }
		tmp = (uint16_t)CPU->A + (uint16_t)CPU->fetched + (uint16_t)NES_6502_getFlag(CPU, FLAG_C);
		NES_6502_setFlag(CPU, FLAG_C, (tmp > 0xFF));
		NES_6502_setFlag(CPU, FLAG_Z, (tmp & 0b0000000011111111) == 0x00);
		NES_6502_setFlag(CPU, FLAG_N, (tmp & 0b10000000));
		// This next line is absolutely hideous, but it makes sense if you work it out on a truth-table
		// Big thanks to javidx9 for publishing a proof and explanation of this derivation on his GitHub/YouTube :)
		partial = (~((uint16_t)CPU->A ^ (uint16_t)CPU->fetched) & ((uint16_t)CPU->A ^ (uint16_t)tmp)) & 0x0080;
		NES_6502_setFlag(CPU, FLAG_V, partial);
		CPU->A = tmp & 0x00FF;
		break;
	case OP_AND:
		if (CPU->page_crossed) { CPU->cycles++; }
		CPU->A = CPU->A & CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_ASL:
		tmp = CPU->fetched << 1;
		NES_6502_setFlag(CPU, FLAG_C, (tmp & 0xFF00) > 0);
		NES_6502_setFlag(CPU, FLAG_Z, (tmp & 0x00FF) == 0x00);
		NES_6502_setFlag(CPU, FLAG_N, (tmp & 0b10000000));
		if (CPU->addr_mode == AM_ACC || CPU->addr_mode == AM_IMP) { CPU->A = tmp & 0x00FF; }
		else {
			// Read-modify-write instructions write the unmodified value back first, then the result on the next cycle
			NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->fetched);
			NES_writeMemory(CPU->bus, CPU->fetched_address, tmp & 0x00FF);
		}
		break;
	case OP_BCC:
		if (NES_6502_getFlag(CPU, FLAG_C) == 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_BCS:
		if (NES_6502_getFlag(CPU, FLAG_C) != 0) { 
			uint16_t oldPC = CPU->PC; 
			CPU->PC = CPU->fetched_address;
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
		}
		break;

	case OP_BEQ:
		if (NES_6502_getFlag(CPU, FLAG_Z) != 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_BIT:
		tmp = CPU->A & CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, ((tmp & 0b0000000011111111) == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, CPU->fetched & 0b10000000);
		NES_6502_setFlag(CPU, FLAG_V, CPU->fetched & 0b01000000);
		break;
	case OP_BMI:
		if (NES_6502_getFlag(CPU, FLAG_N) != 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_BNE:
		if (NES_6502_getFlag(CPU, FLAG_Z) == 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_BPL:
		if (NES_6502_getFlag(CPU, FLAG_N) == 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_BRK:
		// This is a weird one, but this passage from wiki.nesdev.com helps to explain what is happening:
		//
		// "Some 6502 references call this the "B flag", though it does not represent an actual CPU register.
		// Two interrupts (/IRQ and /NMI) and two instructions (PHP and BRK) push the flags to the stack. In the byte pushed, bit 5 is always set to 1, 
		// and bit 4 is 1 if from an instruction (PHP or BRK) or 0 if from an interrupt line being pulled low (/IRQ or /NMI). 
		// This is the only time and place where the B flag actually exists: not in the status register itself, but in bit 4 of the copy that is written to the stack."
		//
		CPU->PC++;								// The byte following the BRK instruction is a padding byte that we must skip over
		// Push the program counter to the stack
		hi = (CPU->PC >> 8) & 0x00FF;
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, hi & 0x00FF);
		CPU->SP--;
		lo = CPU->PC & 0x00FF;
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, lo & 0x00FF);
		CPU->SP--;
		// Set break and unused flags
		NES_6502_setFlag(CPU, FLAG_B, 1);
		NES_6502_setFlag(CPU, FLAG_U, 1);
		// Push the status register to the flag
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->P);
		CPU->SP--;
		// Unset the flags
		NES_6502_setFlag(CPU, FLAG_B, 0);
		NES_6502_setFlag(CPU, FLAG_U, 0);
		NES_6502_setFlag(CPU, FLAG_I, 1);		// wiki.nesdev.com says that a side effect is that the I flag is set to 1 (after P is pushed).
		// Set the program counter to the address stored at the IRQ vector
		lo = NES_readMemory(CPU->bus, NES_6502_IRQ_VECTOR);
		hi = NES_readMemory(CPU->bus, NES_6502_IRQ_VECTOR + 1);
		CPU->PC = (hi << 8) | lo;
		break;
	case OP_BVC:
		if (NES_6502_getFlag(CPU, FLAG_V) == 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_BVS:
		if (NES_6502_getFlag(CPU, FLAG_V) != 0) {
			CPU->cycles++;
			if (CPU->page_crossed) { CPU->cycles++; }
			CPU->PC = CPU->fetched_address;
		}
		break;
	case OP_CLC:
		NES_6502_setFlag(CPU, FLAG_C, 0);
		break;
	case OP_CLD:
		NES_6502_setFlag(CPU, FLAG_D, 0);
		break;
	case OP_CLI:
		NES_6502_setFlag(CPU, FLAG_I, 0);
		break;
	case OP_CLV:
		NES_6502_setFlag(CPU, FLAG_V, 0);
		break;
	case OP_CMP:
		if (CPU->page_crossed) { CPU->cycles++; }
		tmp = (uint16_t)CPU->A - (uint16_t)CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_C, (CPU->A >= CPU->fetched));
		NES_6502_setFlag(CPU, FLAG_Z, ((tmp & 0x00FF) == 0));
		NES_6502_setFlag(CPU, FLAG_N, (tmp & 0x0080));
		break;
	case OP_CPX:
		NES_6502_setFlag(CPU, FLAG_C, (CPU->X >= CPU->fetched));
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->X == CPU->fetched));
		NES_6502_setFlag(CPU, FLAG_N, ((CPU->X - CPU->fetched) & 0b10000000));
		break;
	case OP_CPY:
		NES_6502_setFlag(CPU, FLAG_C, (CPU->Y >= CPU->fetched));
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->Y == CPU->fetched));
		NES_6502_setFlag(CPU, FLAG_N, ((CPU->Y - CPU->fetched) & 0b10000000));
		break;
	case OP_DEC:
		tmp8 = CPU->fetched - 1;
		NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->fetched);
		NES_writeMemory(CPU->bus, CPU->fetched_address, tmp8);
		NES_6502_setFlag(CPU, FLAG_Z, (tmp8 == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (tmp8 & 0b10000000));
		break;
	case OP_DEX:
		CPU->X--;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->X == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->X & 0b10000000));
		break;
	case OP_DEY:
		CPU->Y--;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->Y == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->Y & 0b10000000));
		break;
	case OP_EOR:
		if (CPU->page_crossed) { CPU->cycles++; }
		CPU->A ^= CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_INC:
		tmp8 = CPU->fetched + 1;
		NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->fetched);
		NES_writeMemory(CPU->bus, CPU->fetched_address, tmp8);
		NES_6502_setFlag(CPU, FLAG_Z, (tmp8 == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (tmp8 & 0b10000000));
		break;
	case OP_INX:
		CPU->X++;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->X == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->X & 0b10000000));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->X & 0b10000000));
		break;
	case OP_INY:
		CPU->Y++;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->Y == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->Y & 0b10000000));
		break;
	case OP_JMP:
		CPU->PC = CPU->fetched_address;
		break;
	case OP_JSR:
		CPU->PC--;
		hi = (CPU->PC >> 8) & 0x00FF;
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, hi & 0x00FF);
		CPU->SP--;
		lo = CPU->PC & 0x00FF;
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, lo & 0x00FF);
		CPU->SP--;
		CPU->PC = CPU->fetched_address;
		break;
	case OP_LDA:
		if (CPU->page_crossed) { CPU->cycles++; }
		CPU->A = CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_LDX:
		if (CPU->page_crossed) { CPU->cycles++; }
		CPU->X = CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->X == 0));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->X & 0b10000000));
		break;
	case OP_LDY:
		if (CPU->page_crossed) { CPU->cycles++; }
		CPU->Y = CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->Y == 0));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->Y & 0b10000000));
		break;
	case OP_LSR:
		tmp8 = CPU->fetched >> 1;
		NES_6502_setFlag(CPU, FLAG_C, (CPU->fetched & 0b00000001));
		NES_6502_setFlag(CPU, FLAG_Z, (tmp8 == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (tmp8 & 0b10000000));
		if (CPU->addr_mode == AM_ACC) { CPU->A = tmp8; }
		else {
			NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->fetched);
			NES_writeMemory(CPU->bus, CPU->fetched_address, tmp8);
		}
		break;
	case OP_NOP:
		// Note: There are a handful of unofficial NOP opcodes, and some of them take two clock cycles instead of one
		// This is only the official opcode, with value $EA... It does nothing. :)
		break;
	case OP_ORA:
		if (CPU->page_crossed) { CPU->cycles++; }
		CPU->A |= CPU->fetched;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_PHA:
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->A);
		CPU->SP--;
		break;
	case OP_PHP:
		// Bits 4 and 5 will always be set when pushing the P register to the stack
		NES_6502_setFlag(CPU, FLAG_B, 1);
		NES_6502_setFlag(CPU, FLAG_U, 1);
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->P);
		CPU->SP--;
		NES_6502_setFlag(CPU, FLAG_B, 0);
		NES_6502_setFlag(CPU, FLAG_U, 1);
		break;
	case OP_PLA:
		CPU->SP++;
		// Setting the U and B flags here doesn't appear to match the specifications of the processor... However, the bits don't actually
		// exist on the physical register at all. Setting them to true here allows the emulator to pass the nestest.nes test barrage,
		// which is good: because an actual NES passes all of the tests. So it would seem that this matches the behavior of the NES,
		// regardless of what the specifications say...
		CPU->A = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP);
		NES_6502_setFlag(CPU, FLAG_U, 1); // For nestest.nes
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_PLP:
		// "Two instructions (PLP and RTI) pull a byte from the stack and set all the flags. They ignore bits 5 and 4." - wiki.nesdev.com
		CPU->SP++;
		CPU->P = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP) & 0b11001111 | FLAG_U; // Same deal: setting this flag is programming the emulator against the nestest test cases.
		break;
	case OP_ROL:
		tmp8 = CPU->fetched << 1 | NES_6502_getFlag(CPU, FLAG_C) ;
		NES_6502_setFlag(CPU, FLAG_C, (CPU->fetched & 0b10000000));
		NES_6502_setFlag(CPU, FLAG_Z, (tmp8 == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (tmp8 & 0b10000000));
		if (CPU->addr_mode == AM_ACC) { CPU->A = tmp8; }
		else {
			NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->fetched);
			NES_writeMemory(CPU->bus, CPU->fetched_address, tmp8);
		}
		break;
	case OP_ROR:
		tmp8 = (NES_6502_getFlag(CPU, FLAG_C) << 7) | (CPU->fetched >> 1);
		NES_6502_setFlag(CPU, FLAG_C, (CPU->fetched & 0b00000001));
		NES_6502_setFlag(CPU, FLAG_Z, (tmp8 == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (tmp8 & 0b10000000));
		if (CPU->addr_mode == AM_ACC) { CPU->A = tmp8; }
		else {
			NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->fetched);
			NES_writeMemory(CPU->bus, CPU->fetched_address, tmp8);
		}
		break;
	case OP_RTI:
		// "Two instructions (PLP and RTI) pull a byte from the stack and set all the flags. They ignore bits 5 and 4." - wiki.nesdev.com
		CPU->SP++;
		CPU->P = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP);
		NES_6502_setFlag(CPU, FLAG_U, 1); // For nestest.nes
		NES_6502_setFlag(CPU, FLAG_B, 0); // For nestest.nes
		CPU->SP++;
		lo = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP);
		CPU->SP++;
		hi = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP);
		CPU->PC = (hi << 8) | lo;
		break;
	case OP_RTS:
		CPU->SP++;
		lo = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP);
		CPU->SP++;
		hi = NES_readMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP);
		CPU->PC = (hi << 8) | lo;
		CPU->PC++;
		break;
	case OP_SBC:
		// Formula: A = A - Memory - (1 - FLAG_C)
		// Formula is equivalent to: A + ~Memory + Carry
		// This makes it very similar to the code used for ADC
		if (CPU->page_crossed) { CPU->cycles++; }
		tmp = CPU->A + (CPU->fetched ^ 0x00FF) + NES_6502_getFlag(CPU, FLAG_C);
		NES_6502_setFlag(CPU, FLAG_C, (tmp > 0xFF));
		NES_6502_setFlag(CPU, FLAG_Z, (tmp & 0x00FF) == 0x00);
		NES_6502_setFlag(CPU, FLAG_N, (tmp & 0b10000000));
		NES_6502_setFlag(CPU, FLAG_V, (tmp ^ CPU->A) & (tmp ^ (CPU->fetched ^ 0x00FF)) & 0x0080); // Take a deep breath...
		CPU->A = tmp & 0x00FF;
		break;
	case OP_SEC:
		NES_6502_setFlag(CPU, FLAG_C, 1);
		break;
	case OP_SED:
		NES_6502_setFlag(CPU, FLAG_D, 1);
		break;
	case OP_SEI:
		NES_6502_setFlag(CPU, FLAG_I, 1);
		break;
	case OP_STA:
		NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->A);
		break;
	case OP_STX:
		NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->X);
		break;
	case OP_STY:
		NES_writeMemory(CPU->bus, CPU->fetched_address, CPU->Y);
		break;
	case OP_TAX:
		CPU->X = CPU->A;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->X == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->X & 0b10000000));
		break;
	case OP_TAY:
		CPU->Y = CPU->A;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->Y == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->Y & 0b10000000));
		break;
	case OP_TSX:
		CPU->X = CPU->SP;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->X == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->X & 0b10000000));
		break;
	case OP_TXA:
		CPU->A = CPU->X;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_TXS:
		CPU->SP = CPU->X;
		break;
	case OP_TYA:
		CPU->A = CPU->Y;
		NES_6502_setFlag(CPU, FLAG_Z, (CPU->A == 0x00));
		NES_6502_setFlag(CPU, FLAG_N, (CPU->A & 0b10000000));
		break;
	case OP_XXX:
	default:
		if (DEBUG_ENABLED && myLog != NULL) {
			fflush(myLog);
			fclose(myLog);
			myLog = NULL;
		}
		//printf("Error: Illegal opcodes was found. This is not supported.\n");
		break;
	}
}


// Reset signal handling
void NES_6502_reset(struct Processor* CPU) {
	// Processors is NOT to be zeroed out on reset
	// The behavior of the status flags on reset appears to be undefined behavior on the 6502, but on the NES
	// it appears they go unchanged.
	//CPU->A = 0xFF;
	//CPU->X = 0xFF;
	//CPU->Y = 0xFF;
	//CPU->SP = 0xFD;
	CPU->P = 0x00 | FLAG_I;
	uint16_t lo = NES_readMemory(CPU->bus, NES_6502_RESET_VECTOR);
	uint16_t hi = NES_readMemory(CPU->bus, NES_6502_RESET_VECTOR + 1);
	CPU->PC = (hi << 8) | lo;
	CPU->cycles = 7;
}

// Maskable interrupt signal handling
void NES_6502_irq(struct Processor* CPU) {
	// The interrupt will only be handled if they are not disabled
	if (NES_6502_getFlag(CPU, FLAG_I) == 0) {
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, (CPU->PC >> 8) & 0x00FF);
		CPU->SP--;
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->PC & 0x00FF);
		CPU->SP--;
		NES_6502_setFlag(CPU, FLAG_B, 0);
		NES_6502_setFlag(CPU, FLAG_U, 1);
		NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->P);
		CPU->SP--;
		NES_6502_setFlag(CPU, FLAG_I, 1);
		uint16_t lo = NES_readMemory(CPU->bus, NES_6502_IRQ_VECTOR);
		uint16_t hi = NES_readMemory(CPU->bus, NES_6502_IRQ_VECTOR + 1);
		CPU->PC = (hi << 8) | lo;
		CPU->cycles = 7;
	}
}


// Non-maskable interrupt signal handling
void NES_6502_nmi(struct Processor* CPU) {
	// The same as irq, except this one cannot be ignored because of the I flag
	NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, (CPU->PC >> 8) & 0x00FF);
	CPU->SP--;
	NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->PC & 0x00FF);
	CPU->SP--;
	NES_6502_setFlag(CPU, FLAG_B, 0);
	NES_6502_setFlag(CPU, FLAG_U, 1);
	NES_writeMemory(CPU->bus, NES_6502_STACK_LOCATION + CPU->SP, CPU->P);
	CPU->SP--;
	NES_6502_setFlag(CPU, FLAG_I, 1);
	uint16_t lo = NES_readMemory(CPU->bus, NES_6502_NMI_VECTOR);
	uint16_t hi = NES_readMemory(CPU->bus, NES_6502_NMI_VECTOR + 1);
	CPU->PC = (hi << 8) | lo;
	CPU->cycles = 7;
}


void NES_6502_tickClock(struct Processor* CPU) {

	// Service a pending NMI between instructions (NES_6502_nmi sets up the 7 cycles the interrupt takes)
	if (CPU->cycles == 0 && CPU->nmi_pending) {
		CPU->nmi_pending = false;
		NES_6502_nmi(CPU);
		total_cycles += CPU->cycles;
	}

	// Handle the IRQ line between instructions, unless interrupts are disabled by the I flag
	else if (CPU->cycles == 0 && NES_6502_getFlag(CPU, FLAG_I) == 0 && NES_isIRQAsserted(CPU->bus)) {
		NES_6502_irq(CPU);
		total_cycles += CPU->cycles;
	}

	else if (CPU->cycles == 0) {
		// Fetch the opcode and prepare to execute the next instruction
		CPU->last_pc = CPU->PC;
		CPU->page_crossed = false;
		uint8_t fetchedOpcode = NES_readMemory(CPU->bus, CPU->PC);
		CPU->opcode = charToOpcodeArray[fetchedOpcode];
		CPU->addr_mode = charToAddressModeArray[fetchedOpcode];
		CPU->PC++;

		NES_fetchData(CPU);

		if (DEBUG_ENABLED && myLog != NULL) { printToDebugFile(myLog, CPU); }

		// Get the number of cycles for the fetched opcode and execute it
		CPU->cycles = cyclesArray[fetchedOpcode];
		NES_executeInstruction(CPU);

		if (DEBUG_ENABLED && myLog != NULL) { fprintf(myLog, "%d\n", total_cycles); }

		total_cycles += CPU->cycles;

	}

	// Decrement the CPU's cycle count for multi-cycle instructions
	if (CPU->cycles > 0) { CPU->cycles--; }
	CPU->cycle_count++;
}
